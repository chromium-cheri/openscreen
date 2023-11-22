// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_connection_impl.h"

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "osp/impl/quic/quic_connection_factory_impl.h"
#include "osp/impl/quic/quic_utils.h"
#include "platform/base/error.h"
#include "quiche/quic/core/quic_packets.h"
#include "util/osp_logging.h"
#include "util/trace_logging.h"

namespace openscreen::osp {

PacketWriterImpl::PacketWriterImpl(UdpSocket* socket,
                                   const IPEndpoint& destination)
    : socket_(socket), destination_(destination) {
  OSP_DCHECK(socket_);
}

PacketWriterImpl::~PacketWriterImpl() = default;

quic::WriteResult PacketWriterImpl::WritePacket(
    const char* buffer,
    size_t buffer_length,
    const quic::QuicIpAddress& /*self_address*/,
    const quic::QuicSocketAddress& peer_address,
    quic::PerPacketOptions* /*options*/,
    const quic::QuicPacketWriterParams& /*params*/) {
  OSP_DCHECK_EQ(peer_address, ToQuicSocketAddress(destination_));

  socket_->SendMessage(buffer, buffer_length, destination_);
  OSP_DCHECK_LE(buffer_length,
                static_cast<size_t>(std::numeric_limits<int>::max()));
  return quic::WriteResult(quic::WRITE_STATUS_OK,
                           static_cast<int>(buffer_length));
}

bool PacketWriterImpl::IsWriteBlocked() const {
  return false;
}

void PacketWriterImpl::SetWritable() {}

absl::optional<int> PacketWriterImpl::MessageTooBigErrorCode() const {
  return absl::nullopt;
}

quic::QuicByteCount PacketWriterImpl::GetMaxPacketSize(
    const quic::QuicSocketAddress& /*peer_address*/) const {
  return quic::kMaxOutgoingPacketSize;
}

bool PacketWriterImpl::SupportsReleaseTime() const {
  return false;
}

bool PacketWriterImpl::IsBatchMode() const {
  return false;
}

bool PacketWriterImpl::SupportsEcn() const {
  return false;
}

quic::QuicPacketBuffer PacketWriterImpl::GetNextWriteLocation(
    const quic::QuicIpAddress& /*self_address*/,
    const quic::QuicSocketAddress& /*peer_address*/) {
  return {nullptr, nullptr};
}

quic::WriteResult PacketWriterImpl::Flush() {
  return quic::WriteResult(quic::WRITE_STATUS_OK, 0);
}

QuicStreamImpl::QuicStreamImpl(Delegate* delegate,
                               quic::QuicStreamId id,
                               quic::QuicSession* session,
                               quic::StreamType type)
    : osp::QuicStream(delegate),
      quic::QuicStream(id, session, /*is_static*/ false, type) {}

QuicStreamImpl::~QuicStreamImpl() = default;

uint64_t QuicStreamImpl::GetStreamId() {
  TRACE_SCOPED(TraceCategory::kQuic, "QuicStreamImpl::StreamId");
  return id();
}

void QuicStreamImpl::Write(const uint8_t* data, size_t data_size) {
  TRACE_SCOPED(TraceCategory::kQuic, "QuicStreamImpl::Write");
  OSP_DCHECK(!write_side_closed());
  WriteOrBufferData(
      absl::string_view(reinterpret_cast<const char*>(data), data_size), false,
      nullptr);
}

void QuicStreamImpl::CloseWriteEnd() {
  TRACE_SCOPED(TraceCategory::kQuic, "QuicStreamImpl::CloseWriteEnd");
  CloseWriteSide();
}

void QuicStreamImpl::OnDataAvailable() {
  TRACE_SCOPED(TraceCategory::kQuic, "QuicStreamImpl::OnDataAvailable");
  iovec iov;
  while (!reading_stopped() && sequencer()->GetReadableRegion(&iov)) {
    OSP_DCHECK(!sequencer()->IsClosed());
    delegate_->OnReceived(this, reinterpret_cast<const char*>(iov.iov_base),
                          iov.iov_len);
    sequencer()->MarkConsumed(iov.iov_len);
  }
}

void QuicStreamImpl::OnClose() {
  TRACE_SCOPED(TraceCategory::kQuic, "QuicStreamImpl::OnClose");
  quic::QuicStream::OnClose();
  delegate_->OnClose(GetStreamId());
}

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
                             std::move(quic_packet));
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
