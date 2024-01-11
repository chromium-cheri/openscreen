// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_connection_impl.h"

#include "osp/impl/quic/quic_connection_factory_impl.h"
#include "osp/impl/quic/quic_utils.h"
#include "platform/base/error.h"
#include "quiche/quic/core/quic_packets.h"
#include "util/osp_logging.h"
#include "util/trace_logging.h"

namespace openscreen::osp {

QuicConnectionImpl::QuicConnectionImpl(
    QuicConnectionFactoryImpl* parent_factory,
    QuicConnection::Delegate* delegate,
    const quic::QuicClock* clock)
    : QuicConnection(delegate), parent_factory_(parent_factory), clock_(clock) {
  TRACE_SCOPED(TraceCategory::kQuic, "QuicConnectionImpl::QuicConnectionImpl");
}

QuicConnectionImpl::~QuicConnectionImpl() = default;

// Passes a received UDP packet to the QUIC implementation.  If this contains
// any stream data, it will be passed automatically to the relevant
// QuicStreamImpl objects.
void QuicConnectionImpl::OnRead(UdpSocket* socket, ErrorOr<UdpPacket> packet) {
  TRACE_SCOPED(TraceCategory::kQuic, "QuicConnectionImpl::OnRead");
  if (packet.is_error()) {
    TRACE_SET_RESULT(packet.error());
    return;
  }

  quic::QuicReceivedPacket quic_packet(
      reinterpret_cast<const char*>(packet.value().data()),
      packet.value().size(), clock_->Now());
  session_->ProcessUdpPacket(ToQuicSocketAddress(socket->GetLocalEndpoint()),
                             ToQuicSocketAddress(packet.value().source()),
                             quic_packet);
}

void QuicConnectionImpl::OnSendError(UdpSocket* socket, Error error) {
  // TODO(crbug.com/openscreen/67): Implement this method.
  OSP_UNIMPLEMENTED();
}

void QuicConnectionImpl::OnError(UdpSocket* socket, Error error) {
  // TODO(crbug.com/openscreen/67): Implement this method.
  OSP_UNIMPLEMENTED();
}

QuicStream* QuicConnectionImpl::MakeOutgoingStream(
    QuicStream::Delegate* delegate) {
  TRACE_SCOPED(TraceCategory::kQuic, "QuicConnectionImpl::MakeOutgoingStream");
  OSP_DCHECK(session_);

  return session_->CreateOutgoingStream(delegate);
}

void QuicConnectionImpl::Close() {
  TRACE_SCOPED(TraceCategory::kQuic, "QuicConnectionImpl::Close");
  session_->connection()->CloseConnection(
      quic::QUIC_PEER_GOING_AWAY, "session torn down",
      quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
}

void QuicConnectionImpl::OnConnectionClosed(
    quic::QuicConnectionId server_connection_id,
    quic::QuicErrorCode error_code,
    const std::string& error_details,
    quic::ConnectionCloseSource source) {
  OSP_LOG_INFO << "QuicConnection is closed, reason is: " << error_details;
  TRACE_SCOPED(TraceCategory::kQuic, "QuicConnectionImpl::OnConnectionClosed");
  parent_factory_->OnConnectionClosed(this);
  delegate_->OnConnectionClosed(session_->connection_id().ToString());
}

void QuicConnectionImpl::OnCryptoHandshakeComplete() {
  TRACE_SCOPED(TraceCategory::kQuic,
               "QuicConnectionImpl::OnCryptoHandshakeComplete");
  delegate_->OnCryptoHandshakeComplete(session_->connection_id().ToString());
}

void QuicConnectionImpl::OnIncomingStream(QuicStream* stream) {
  TRACE_SCOPED(TraceCategory::kQuic, "QuicConnectionImpl::OnIncomingStream");
  delegate_->OnIncomingStream(session_->connection_id().ToString(), stream);
}

}  // namespace openscreen::osp
