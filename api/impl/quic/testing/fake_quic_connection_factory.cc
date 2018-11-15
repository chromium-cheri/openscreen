// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/quic/testing/fake_quic_connection_factory.h"

#include <algorithm>

#include "base/make_unique.h"
#include "platform/api/logging.h"

namespace openscreen {

RunAsReceiver::RunAsReceiver(FakeQuicConnectionFactory* factory)
    : factory_(factory) {
  factory_->SwitchEndpointContexts();
}

RunAsReceiver::~RunAsReceiver() {
  factory_->SwitchEndpointContexts();
}

FakeQuicConnectionFactory::FakeQuicConnectionFactory(
    const IPEndpoint& local_endpoint,
    const IPEndpoint& remote_endpoint,
    MessageDemuxer* local_demuxer,
    MessageDemuxer* remote_demuxer)
    : local_demuxer_(local_demuxer),
      remote_demuxer_(remote_demuxer),
      local_endpoint_(local_endpoint),
      remote_endpoint_(remote_endpoint) {}

FakeQuicConnectionFactory::~FakeQuicConnectionFactory() = default;

void FakeQuicConnectionFactory::SwitchEndpointContexts() {
  using std::swap;
  swap(local_demuxer_, remote_demuxer_);
  swap(local_endpoint_, remote_endpoint_);
  remote_endpoint_index_ = !remote_endpoint_index_;
  MessageDemuxer::Set(nullptr);
  MessageDemuxer::Set(local_demuxer_);
}

void FakeQuicConnectionFactory::RunTasksUntilIdle() {
  do {
    RunTasks();
  } while (!idle_);
}

void FakeQuicConnectionFactory::SetServerDelegate(ServerDelegate* delegate,
                                                  IPAddress::Version ip_version,
                                                  uint16_t port) {
  server_delegate_ = delegate;
}

void FakeQuicConnectionFactory::RunTasks() {
  idle_ = true;
  for (auto& connection_with_id : connections_) {
    for (auto& stream : connection_with_id.connection->streams()) {
      std::vector<uint8_t> received_data = stream.second->TakeReceivedData();
      std::vector<uint8_t> written_data = stream.second->TakeWrittenData();
      if (received_data.size()) {
        idle_ = false;
        stream.second->delegate()->OnReceived(
            stream.second, reinterpret_cast<const char*>(received_data.data()),
            received_data.size());
      }
      if (written_data.size()) {
        RunAsReceiver as_remote{this};
        idle_ = false;
        local_demuxer_->OnStreamData(
            remote_endpoint_, stream.second,
            reinterpret_cast<const char*>(written_data.data()),
            written_data.size());
      }
    }
  }
  for (auto& connection_with_id : pending_connections_) {
    idle_ = false;
    connection_with_id.connection->delegate()->OnCryptoHandshakeComplete(
        next_connection_id_++);
    connections_.push_back(connection_with_id);
  }
  pending_connections_.clear();
}

std::unique_ptr<QuicConnection> FakeQuicConnectionFactory::Connect(
    const IPEndpoint& endpoint,
    QuicConnection::Delegate* connection_delegate) {
  OSP_DCHECK_EQ(endpoint, remote_endpoint_);
  OSP_DCHECK(
      std::find_if(
          pending_connections_.begin(), pending_connections_.end(),
          [&endpoint, this](const ConnectionWithIdentity& connection_with_id) {
            return connection_with_id.endpoints[this->remote_endpoint_index_] ==
                   endpoint;
          }) == pending_connections_.end());
  auto connection = MakeUnique<FakeQuicConnection>(connection_delegate);
  ConnectionWithIdentity connection_with_id{{}, connection.get()};
  connection_with_id.endpoints[remote_endpoint_index_] = endpoint;
  connection_with_id.endpoints[!remote_endpoint_index_] = local_endpoint_;
  pending_connections_.push_back(connection_with_id);
  return connection;
}

void FakeQuicConnectionFactory::OnConnectionClosed(QuicConnection* connection) {
  for (auto entry = connections_.begin(); entry != connections_.end();
       ++entry) {
    if (entry->connection == connection) {
      connections_.erase(entry);
      return;
    }
  }
  OSP_DCHECK(false) << "reporting an unknown connection as closed";
}

}  // namespace openscreen
