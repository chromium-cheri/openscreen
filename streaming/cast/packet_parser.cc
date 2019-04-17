// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "streaming/cast/packet_parser.h"

#include <algorithm>

#include "platform/api/logging.h"
#include "streaming/cast/big_endian.h"

namespace openscreen {
namespace cast_streaming {

namespace {

// Name aliases for the return values from the ParseXYZ() methods.
constexpr bool kCorrupted = false;
constexpr bool kWellFormed = true;

}  // namespace

PacketParser::PacketParser(Ssrc local_ssrc, Ssrc remote_ssrc)
    : local_ssrc_(local_ssrc),
      remote_ssrc_(remote_ssrc),
      highest_rtp_frame_id_(FrameId::first()) {}

PacketParser::~PacketParser() = default;

bool PacketParser::Parse(absl::Span<const uint8_t> packet) {
  // Reset.
  rtp_header_ = absl::nullopt;
  rtp_payload_ = {};

  // Determine whether to attempt to parse as a RTP packet or a RTCP packet.
  // See wire-format diagram in ParseRtp() for details.
  if (packet.size() < 2) {
    return kCorrupted;  // Insufficient data for packet to be either kind.
  }
  if (IsRtpPayloadType(packet[1] & 0b01111111)) {
    return ParseRtp(packet);
  }
  // TODO(miu): Check for and parse RTCP packets.
  return kCorrupted;
}

bool PacketParser::ParseRtp(absl::Span<const uint8_t> packet) {
  //  0                   1                   2                   3
  //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ ^
  // |V  |P|X| CC=0  |M|      PT     |      sequence number          | |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+RTP
  // +                         RTP timestamp                         |Spec
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ |
  // +         synchronization source (SSRC) identifier              | v
  // +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  // |K|R| EXT count |  FID          |              PID              | ^
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+Cast
  // |             Max PID           |  optional fields, extensions,  Spec
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+  then payload...                v
  if (packet.size() < 18 || packet[0] != kRtpRequiredFirstByte) {
    return kCorrupted;
  }
  // Note: M (marker bit) is ignored here. Technically, according to the Cast
  // Streaming spec, it should only be set when PID == Max PID; but, let's be
  // lenient just in case some sender implementations don't adhere to this tiny,
  // subtle detail.
  if (ReadBigEndian<uint32_t>(packet.data() + 8) != remote_ssrc_) {
    return kWellFormed;  // Ignore RTP packet from unknown sender.
  }

  rtp_header_.emplace();

  // RTP header elements.
  const uint8_t raw_payload_type = packet[1] & 0b01111111;
  // This method should not have been called without the caller having validated
  // the payload type already.
  OSP_DCHECK(IsRtpPayloadType(raw_payload_type));
  rtp_header_->payload_type = static_cast<RtpPayloadType>(raw_payload_type);
  rtp_header_->sequence_number = ReadBigEndian<uint16_t>(packet.data() + 2);
  rtp_header_->rtp_timestamp = last_parsed_rtp_timestamp_.Expand(
      ReadBigEndian<uint32_t>(packet.data() + 4));

  // Cast-specific header elements.
  rtp_header_->is_key_frame = !!(packet[12] & 0b10000000);
  const bool has_referenced_frame_id = !!(packet[12] & 0b01000000);
  const size_t num_cast_extensions = packet[12] & 0b00111111;
  rtp_header_->frame_id = highest_rtp_frame_id_.Expand(packet[13]);
  rtp_header_->packet_id = ReadBigEndian<uint16_t>(packet.data() + 14);
  rtp_header_->max_packet_id = ReadBigEndian<uint16_t>(packet.data() + 16);
  if (rtp_header_->packet_id > rtp_header_->max_packet_id) {
    return kCorrupted;
  }
  if (has_referenced_frame_id) {
    if (packet.size() < 19) {
      return kCorrupted;
    }
    rtp_header_->referenced_frame_id = rtp_header_->frame_id.Expand(packet[18]);
    packet.remove_prefix(19);
  } else {
    // By default, if no reference frame ID was provided, the assumption is that
    // a key frame only references itself, while non-key frames reference only
    // their immediate predecessor.
    rtp_header_->referenced_frame_id = rtp_header_->is_key_frame
                                           ? rtp_header_->frame_id
                                           : (rtp_header_->frame_id - 1);
    packet.remove_prefix(18);
  }

  // Cast extensions. This implementation supports only the Adaptive Latency
  // extension, and ignores all others. Thus, the wire-format parsed here is:
  //
  //  0                   1                   2                   3
  //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |  TYPE = 1 | Ext data SIZE = 2 |Playout Delay (unsigned millis)|
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  for (size_t i = 0; i < num_cast_extensions; ++i) {
    if (packet.size() < 2) {
      return kCorrupted;
    }
    const uint16_t type_and_size = ReadBigEndian<uint16_t>(packet.data());
    packet.remove_prefix(2);
    const uint8_t type = type_and_size >> 10;
    const size_t size = type_and_size & 0x3ff;
    if (packet.size() < size) {
      return kCorrupted;
    }
    if (type == kAdaptiveLatencyRtpExtensionType) {
      rtp_header_->new_playout_delay =
          std::chrono::milliseconds(ReadBigEndian<uint16_t>(packet.data()));
    }
    packet.remove_prefix(size);
  }

  // All remaining data in the packet is the payload.
  rtp_payload_ = packet;

  // At this point, the packet is known to be well-formed. Track recent field
  // values for later parses, to bit-extend the truncated values found in future
  // packets.
  last_parsed_rtp_timestamp_ = rtp_header_->rtp_timestamp;
  highest_rtp_frame_id_ =
      std::max(highest_rtp_frame_id_, rtp_header_->frame_id);

  return kWellFormed;
}

bool PacketParser::ParseRtcp(absl::Span<const uint8_t> packet) {
  OSP_UNIMPLEMENTED();
  (void)local_ssrc_;  // TODO(miu): Member will be used in upcoming CL.
  return kCorrupted;
}

// static
bool PacketParser::IsPacketFromPeer(absl::Span<const uint8_t> packet,
                                    Ssrc peer_ssrc) {
  if (packet.size() < 2) {
    return false;
  }

  if (IsRtpPayloadType(packet[1] & 0b01111111)) {
    // See ParseRtp() for wire-format diagram.
    if (packet.size() < 12) {
      return false;
    }
    if (packet[0] != kRtpRequiredFirstByte) {
      return false;
    }
    return ReadBigEndian<uint32_t>(packet.data() + 8) == peer_ssrc;
  }

  return false;
}

}  // namespace cast_streaming
}  // namespace openscreen
