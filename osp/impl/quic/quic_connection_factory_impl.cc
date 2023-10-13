// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_connection_factory_impl.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

#include "osp/impl/quic/open_screen_client_session.h"
#include "osp/impl/quic/open_screen_server_session.h"
#include "osp/impl/quic/quic_alarm_factory_impl.h"
#include "osp/impl/quic/quic_connection_impl.h"
#include "osp/impl/quic/quic_utils.h"
#include "platform/api/task_runner.h"
#include "platform/api/time.h"
#include "platform/base/error.h"
#include "quiche/common/quiche_random.h"
#include "quiche/quic/core/crypto/quic_compressed_certs_cache.h"
#include "quiche/quic/core/quic_default_connection_helper.h"
#include "quiche/quic/core/quic_utils.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "util/osp_logging.h"
#include "util/std_util.h"
#include "util/trace_logging.h"

namespace openscreen::osp {

namespace {

const char kSourceAddressTokenSecret[] = "secret";

}  // namespace

QuicConnectionFactoryImpl::QuicConnectionFactoryImpl(TaskRunner& task_runner,
                                                     bool is_for_client)
    : compressed_certs_cache_(
          quic::QuicCompressedCertsCache::kQuicCompressedCertsCacheSize),
      task_runner_(task_runner) {
  helper_ = std::make_unique<quic::QuicDefaultConnectionHelper>();
  supported_versions_ =
      quic::ParsedQuicVersionVector{quic::ParsedQuicVersion::RFCv1()};
  alarm_factory_ = std::make_unique<QuicAlarmFactoryImpl>(
      task_runner, quic::QuicDefaultClock::Get());
  if (is_for_client) {
    // TODO: Determine quic::ProofVerifier.
    client_config = std::make_unique<quic::QuicCryptoClientConfig>(nullptr);
  } else {
    // TODO: Determine quic::ProofSource.
    server_config = std::make_unique<quic::QuicCryptoServerConfig>(
        kSourceAddressTokenSecret, quic::QuicRandom::GetInstance(), nullptr,
        quic::KeyExchangeSource::Default());
  }
}

QuicConnectionFactoryImpl::~QuicConnectionFactoryImpl() {
  OSP_DCHECK(connections_.empty());
}

void QuicConnectionFactoryImpl::SetServerDelegate(
    ServerDelegate* delegate,
    const std::vector<IPEndpoint>& endpoints) {
  OSP_DCHECK(!delegate != !server_delegate_);

  server_delegate_ = delegate;
  sockets_.reserve(sockets_.size() + endpoints.size());

  for (const auto& endpoint : endpoints) {
    // TODO(mfoltz): Need to notify the caller and/or ServerDelegate if socket
    // create/bind errors occur. Maybe return an Error immediately, and undo
    // partial progress (i.e. "unwatch" all the sockets and call
    // sockets_.clear() to close the sockets)?
    auto create_result = UdpSocket::Create(task_runner_, this, endpoint);
    if (!create_result) {
      OSP_LOG_ERROR << "failed to create socket (for " << endpoint
                    << "): " << create_result.error().message();
      continue;
    }
    std::unique_ptr<UdpSocket> server_socket = std::move(create_result.value());
    server_socket->Bind();
    sockets_.emplace_back(std::move(server_socket));
  }
}

void QuicConnectionFactoryImpl::OnError(UdpSocket* socket, Error error) {
  OSP_LOG_ERROR << "failed to configure socket " << error.message();
}

void QuicConnectionFactoryImpl::OnSendError(UdpSocket* socket, Error error) {
  // TODO(crbug.com/openscreen/67): Implement this method.
  OSP_UNIMPLEMENTED();
}

void QuicConnectionFactoryImpl::OnRead(UdpSocket* socket,
                                       ErrorOr<UdpPacket> packet_or_error) {
  TRACE_SCOPED(TraceCategory::kQuic, "QuicConnectionFactoryImpl::OnRead");
  if (packet_or_error.is_error()) {
    TRACE_SET_RESULT(packet_or_error.error());
    return;
  }

  UdpPacket packet = std::move(packet_or_error.value());
  // TODO(btolsch): We will need to rethink this both for ICE and connection
  // migration support.
  auto conn_it = connections_.find(packet.source());
  if (conn_it == connections_.end()) {
    if (server_delegate_) {
      OSP_VLOG << __func__ << ": spawning connection from " << packet.source();
      quic::QuicPacketWriter* writer =
          new PacketWriterImpl(socket, packet.source());
      quic::QuicConnectionId connection_id =
          quic::QuicUtils::CreateRandomConnectionId(
              helper_->GetRandomGenerator());
      auto connection = std::make_unique<quic::QuicConnection>(
          connection_id, quic::QuicSocketAddress(),
          ToQuicSocketAddress(packet.source()), helper_.get(),
          alarm_factory_.get(), writer, /* owns_writer */ true,
          quic::Perspective::IS_SERVER, supported_versions_,
          connection_id_generator_);

      auto connection_impl = std::make_unique<QuicConnectionImpl>(
          this, server_delegate_->NextConnectionDelegate(packet.source()),
          helper_->GetClock());
      auto* connection_impl_ptr = connection_impl.get();

      std::unique_ptr<OpenScreenSessionBase> session =
          std::make_unique<OpenScreenServerSession>(
              std::move(connection), server_config.get(), connection_impl_ptr,
              config_, supported_versions_, &compressed_certs_cache_);
      connection_impl->set_session(std::move(session));

      connections_.emplace(packet.source(),
                           OpenConnection{connection_impl_ptr, socket});
      server_delegate_->OnIncomingConnection(std::move(connection_impl));
      connection_impl_ptr->OnRead(socket, std::move(packet));
    } else {
      OSP_VLOG << __func__ << ": data for existing connection from "
               << packet.source();
      conn_it->second.connection->OnRead(socket, std::move(packet));
    }
  }
}

std::unique_ptr<QuicConnection> QuicConnectionFactoryImpl::Connect(
    const IPEndpoint& endpoint,
    QuicConnection::Delegate* connection_delegate) {
  auto create_result = UdpSocket::Create(task_runner_, this, endpoint);
  if (!create_result) {
    OSP_LOG_ERROR << "failed to create socket: "
                  << create_result.error().message();
    // TODO(mfoltz): This method should return ErrorOr<uni_ptr<QuicConnection>>.
    return nullptr;
  }
  auto socket = std::move(create_result.value());
  quic::QuicPacketWriter* writer = new PacketWriterImpl(socket.get(), endpoint);
  quic::QuicConnectionId connection_id =
      quic::QuicUtils::CreateRandomConnectionId(helper_->GetRandomGenerator());
  auto connection = std::make_unique<quic::QuicConnection>(
      connection_id, quic::QuicSocketAddress(), ToQuicSocketAddress(endpoint),
      helper_.get(), alarm_factory_.get(), writer, /* owns_writer */ true,
      quic::Perspective::IS_CLIENT, supported_versions_,
      connection_id_generator_);

  auto connection_impl = std::make_unique<QuicConnectionImpl>(
      this, connection_delegate, helper_->GetClock());

  std::unique_ptr<OpenScreenSessionBase> session =
      std::make_unique<OpenScreenClientSession>(
          std::move(connection), client_config.get(), connection_impl.get(),
          config_, supported_versions_, server_id_);
  connection_impl->set_session(std::move(session));

  // TODO(btolsch): This presents a problem for multihomed receivers, which may
  // register as a different endpoint in their response.  I think QUIC is
  // already tolerant of this via connection IDs but this hasn't been tested
  // (and even so, those aren't necessarily stable either).
  connections_.emplace(endpoint,
                       OpenConnection{connection_impl.get(), socket.get()});
  sockets_.emplace_back(std::move(socket));

  return connection_impl;
}

void QuicConnectionFactoryImpl::OnConnectionClosed(QuicConnection* connection) {
  auto entry = std::find_if(
      connections_.begin(), connections_.end(),
      [connection](const decltype(connections_)::value_type& entry) {
        return entry.second.connection == connection;
      });
  OSP_DCHECK(entry != connections_.end());
  UdpSocket* const socket = entry->second.socket;
  connections_.erase(entry);

  // If none of the remaining |connections_| reference the socket, close/destroy
  // it.
  if (!ContainsIf(connections_,
                  [socket](const decltype(connections_)::value_type& entry) {
                    return entry.second.socket == socket;
                  })) {
    auto socket_it =
        std::find_if(sockets_.begin(), sockets_.end(),
                     [socket](const std::unique_ptr<UdpSocket>& s) {
                       return s.get() == socket;
                     });
    OSP_DCHECK(socket_it != sockets_.end());
    sockets_.erase(socket_it);
  }
}

}  // namespace openscreen::osp
