// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_packet_writer_impl.h"

#include <limits>

#include "osp/impl/quic/quic_utils.h"
#include "util/osp_logging.h"

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

  auto source_address = socket_->GetLocalEndpoint().ToString();
  auto destination_address = destination_.ToString();
  OSP_LOG_INFO << "---PacketWriter writes packet, source endpoint is: "
               << source_address
               << "; destination endpoint is: " << destination_address;
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

}  // namespace openscreen::osp
