// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "streaming/cast/rtp_packetizer.h"

#include <limits>
#include <random>

#include "platform/api/logging.h"
#include "platform/api/time.h"
#include "streaming/cast/big_endian.h"

namespace openscreen {
namespace cast_streaming {

namespace {

// See wire-format diagrams in GeneratePacket(), below.
constexpr int kBaseRtpHeaderSize = 19;
constexpr int kAdaptiveLatencyHeaderSize = 4;
constexpr int kMaxRtpHeaderSize =
    kBaseRtpHeaderSize + kAdaptiveLatencyHeaderSize;

// Start with the max packet size, then subtract reserved space for packet
// header fields. The rest can be allocated to the payload.
constexpr int kMaxPayloadSize = kMaxRtpPacketSize - kMaxRtpHeaderSize;

uint16_t GenerateRandomSequenceNumberStart() {
  // Use a statically-allocated generator, instantiated upon first use, and
  // seeded with the current time tick count. This generator was chosen because
  // it is light-weight and does not need to produce unguessable (nor
  // crypto-secure) values.
  static std::minstd_rand generator(static_cast<std::minstd_rand::result_type>(
      platform::Clock::now().time_since_epoch().count()));

  return std::uniform_int_distribution<uint16_t>()(generator);
}

}  // namespace

RtpPacketizer::RtpPacketizer(RtpPayloadType payload_type, Ssrc ssrc)
    : payload_type_7bits_(static_cast<uint8_t>(payload_type)),
      ssrc_(ssrc),
      sequence_number_(GenerateRandomSequenceNumberStart()) {
  // Payload type must be an integer representable in 7 bits.
  OSP_DCHECK_EQ(payload_type_7bits_, payload_type_7bits_ & 0b01111111);
  OSP_DCHECK(IsRtpPayloadType(payload_type_7bits_));
}

RtpPacketizer::~RtpPacketizer() = default;

absl::Span<uint8_t> RtpPacketizer::GeneratePacket(const EncryptedFrame& frame,
                                                  PacketId packet_id,
                                                  absl::Span<uint8_t> buffer) {
  const int num_packets = ComputeNumberOfPackets(frame);
  OSP_DCHECK_LT(int{packet_id}, num_packets);
  const bool is_last_packet = int{packet_id} == (num_packets - 1);

  // Compute the size of this packet, which is the number of bytes of header
  // plus the number of bytes of payload. Note that the optional Adaptive
  // Latency information is only added to the first packet.
  int packet_size = kBaseRtpHeaderSize;
  const bool include_adaptive_latency_change =
      (packet_id == 0 &&
       frame.new_playout_delay > std::chrono::milliseconds(0));
  if (include_adaptive_latency_change) {
    OSP_DCHECK_LE(frame.new_playout_delay.count(),
                  int{std::numeric_limits<uint16_t>::max()});
    packet_size += kAdaptiveLatencyHeaderSize;
  }
  int data_chunk_size = kMaxPayloadSize;
  const int data_chunk_start = data_chunk_size * int{packet_id};
  if (is_last_packet) {
    data_chunk_size = static_cast<int>(frame.data.size()) - data_chunk_start;
  }
  packet_size += data_chunk_size;

  if (static_cast<int>(buffer.size()) < packet_size) {
    return {};  // Failure: The buffer is not large enough.
  }
  const absl::Span<uint8_t> packet{buffer.data(),
                                   static_cast<size_t>(packet_size)};
  OSP_DCHECK_LE(packet.size(), kMaxRtpPacketSize);

  // RTP Header.
  //
  //  0                   1                   2                   3
  //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |V=2|P|X| CC=0  |M|      PT     |      sequence number          |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // +                           timestamp                           |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // +         synchronization source (SSRC) identifier              |
  // +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  //
  // Byte 0: Version 2, no padding, no RTP extensions, no CSRCs.
  packet[0] = kRtpRequiredFirstByte;
  // Byte 1: Marker bit indicates whether this is the last packet, followed by
  // a 7-bit payload type.
  packet[1] = (is_last_packet ? 0b10000000 : 0) | payload_type_7bits_;
  WriteBigEndian<uint16_t>(sequence_number_++, packet.data() + 2);
  WriteBigEndian<uint32_t>(frame.rtp_timestamp.lower_32_bits(),
                           packet.data() + 4);
  WriteBigEndian<uint32_t>(ssrc_, packet.data() + 8);

  // Cast Header.
  //
  //  0                   1                   2                   3
  //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  // |K|R| EXT count |  FID          |              PID              |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |             Max PID           |     RFID      |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //
  // Byte 12: Key Frame bit, followed by "RFID will be provided" bit, followed
  // by 6 bits specifying the number of extensions that will be provided.
  packet[12] =
      ((frame.dependency == EncodedFrame::KEY) ? 0b11000000 : 0b01000000) |
      (include_adaptive_latency_change ? 1 : 0);
  packet[13] = frame.frame_id.lower_8_bits();
  WriteBigEndian<uint16_t>(packet_id, packet.data() + 14);
  WriteBigEndian<uint16_t>(num_packets - 1, packet.data() + 16);
  packet[18] = frame.referenced_frame_id.lower_8_bits();
  uint8_t* header_end = packet.data() + kBaseRtpHeaderSize;

  // Extension of Cast Header for Adaptive Latency change.
  //
  //  0                   1                   2                   3
  //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |  TYPE = 1 | Ext data SIZE = 2 |Playout Delay (unsigned millis)|
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  if (include_adaptive_latency_change) {
    constexpr uint16_t kSizeOfPlayoutDelayField = sizeof(uint16_t);
    WriteBigEndian<uint16_t>(
        (kAdaptiveLatencyRtpExtensionType << 10) | kSizeOfPlayoutDelayField,
        header_end);
    WriteBigEndian<uint16_t>(frame.new_playout_delay.count(), header_end + 2);
    header_end += 2 * sizeof(uint16_t);
  }

  // Sanity-check the pointer math, to ensure the packet is being entirely
  // populated, with no underrun or overrun.
  OSP_DCHECK_EQ(header_end + data_chunk_size, &*packet.end());

  // Copy the encrypted payload data into the packet.
  memcpy(header_end, frame.data.data() + data_chunk_start, data_chunk_size);

  return packet;
}

// static
int RtpPacketizer::ComputeNumberOfPackets(const EncryptedFrame& frame) {
  // The total number of packets is computed by assuming the payload will be
  // split-up across as few packets as possible.
  int num_packets =
      (static_cast<int>(frame.data.size()) + (kMaxPayloadSize - 1)) /
      kMaxPayloadSize;
  // Edge case: There must always be at least one packet, even when there are no
  // payload bytes.
  num_packets = std::max(1, num_packets);

  // The result must be strictly less than the max possible PacketId value to
  // avoid using the "special" PacketId (0xffff) used by the RTCP ACK/NACK
  // scheme.
  OSP_DCHECK_LT(num_packets, int{std::numeric_limits<PacketId>::max()});
  return num_packets;
}

}  // namespace cast_streaming
}  // namespace openscreen
