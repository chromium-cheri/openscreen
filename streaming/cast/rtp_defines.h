// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_RTP_DEFINES_H_
#define STREAMING_CAST_RTP_DEFINES_H_

#include <stdint.h>

#include <chrono>

#include "streaming/cast/frame_id.h"
#include "streaming/cast/rtp_time.h"

namespace openscreen {
namespace cast_streaming {

// Note: Cast Streaming uses a subset of the messages in the RTP/RTCP
// specification, but also adds some of its own extensions. See:
// https://tools.ietf.org/html/rfc3550

// Uniquely identifies one packet within a frame.
using PacketId = uint16_t;

// The maximum size of any RTP or RTCP packet, in bytes. The calculation below
// is: Ethernet MTU bytes minus IP header minus UDP header. The remainder is
// available for RTP/RTCP packet data (header + payload).
//
// A nice explanation of this: https://jvns.ca/blog/2017/02/07/mtu/
constexpr size_t kMaxRtpPacketSize = 1500 - 20 - 8;

// All RTP packets must carry the version 2 flag, not use padding, not use RTP
// extensions, and have zero CSRCs.
constexpr uint8_t kRtpRequiredFirstByte = 0b10000000;

// Describes the content being transported over RTP streams. These are
// Cast Streaming specific assignments, within the "dynamic" range provided by
// IANA.
enum class RtpPayloadType : uint8_t {
  kNull = 0,

  kAudioFirst = 96,

  // Cast Streaming will encode raw audio frames using one of its available
  // codec implementations, and transport encoded data in the RTP stream.
  kAudioOpus = 96,
  kAudioAac = 97,
  kAudioPcm16 = 98,

  // Audio frame data is not modified, and should be transported reliably and
  // in-sequence. No assumptions about the data can be made.
  kRemoteAudio = 99,

  kAudioLast = kRemoteAudio,

  // Cast Streaming will encode raw video frames using one of its available
  // codec implementations, and transport encoded data in the RTP stream.
  kVideoVp8 = 100,
  kVideoH264 = 101,

  // Video frame data is not modified, and should be transported reliably and
  // in-sequence. No assumptions about the data can be made.
  kRemoteVideo = 102,

  kVideoLast = 102,
};

// Returns true if the |raw_byte| can be casted to a RtpPayloadType, and is also
// not RtpPayloadType::kNull. The caller should mask the byte, to select the
// lower 7 bits, if applicable.
bool IsRtpPayloadType(uint8_t raw_byte);

// Cast Streaming RTP extension: Permits changing the fixed end-to-end latency
// of a stream during a session.
constexpr uint8_t kAdaptiveLatencyRtpExtensionType = 1;

// https://tools.ietf.org/html/rfc3550#section-5
struct RtpCastHeader {
  RtpCastHeader();
  ~RtpCastHeader();

  // Elements from RTP packet header.
  RtpPayloadType payload_type;
  uint16_t sequence_number;    // Wrap-around packet transmission counter.
  RtpTimeTicks rtp_timestamp;  // The media timestamp.

  // Elements from Cast header (at beginning of RTP payload).
  bool is_key_frame;
  FrameId frame_id;
  PacketId packet_id;  // Always in the range [0,max_packet_id].
  PacketId max_packet_id;
  FrameId referenced_frame_id;  // ID of the frame required to decode this one.
  std::chrono::milliseconds new_playout_delay{};  // Ignore if non-positive.
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_RTP_DEFINES_H_
