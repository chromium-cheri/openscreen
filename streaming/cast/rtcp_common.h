// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_RTCP_COMMON_H_
#define STREAMING_CAST_RTCP_COMMON_H_

#include <stdint.h>

#include <vector>

#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "streaming/cast/ntp_time.h"
#include "streaming/cast/rtp_defines.h"
#include "streaming/cast/rtp_time.h"
#include "streaming/cast/ssrc.h"

namespace openscreen {
namespace cast_streaming {

struct RtcpCommonHeader {
  RtcpCommonHeader();
  ~RtcpCommonHeader();

  RtcpPacketType packet_type = RtcpPacketType::kNull;

  // The number of report blocks if |packet_type| is kSenderReport or
  // kReceiverReport. Otherwise, not used.
  int item_count = 0;

  // Indicates the type of an application-defined message if |packet_type| is
  // kApplicationDefined or kPayloadSpecific. Otherwise, not used.
  RtcpSubtype subtype = RtcpSubtype::kNull;

  // The size (in bytes) of the RTCP packet, not including the header.
  int size = 0;

  // Serializes this header into the first |kRtcpCommonHeaderSize| bytes of the
  // given |buffer|.
  void Serialize(absl::Span<uint8_t> buffer) const;

  // Parse from the 4-byte wire format in |buffer|. Returns nullopt if the data
  // is corrupt.
  static absl::optional<RtcpCommonHeader> Parse(
      absl::Span<const uint8_t> buffer);
};

// The middle 32-bits of the 64-bit NtpTimestamp field from the Sender Reports.
// This is used as an opaque identifier that the Receiver will use in its
// reports to refer to specific previous Sender Reports.
using StatusReportId = uint32_t;
constexpr StatusReportId ToStatusReportId(NtpTimestamp ntp_timestamp) {
  return static_cast<uint32_t>(ntp_timestamp >> 16);
}

// One of these is optionally included with a Sender Report or a Receiver
// Report.
struct RtcpReportBlock {
  RtcpReportBlock();
  ~RtcpReportBlock();

  // The intended recipient of this report block.
  Ssrc recipient_ssrc = 0;

  // The fraction of RTP packets lost since the last report, as an 8-bit fixed
  // point value (i.e., each unit increment represents an additional 1/256).
  uint8_t packet_fraction_lost = 0;

  // The total number of RTP packets lost since the start of the session. This
  // value will always be in the range [0,2^24-1], as the wire format only
  // provides 24 bits; so, wrap-around is possible.
  uint32_t cumulative_packets_lost = 0;

  // The highest sequence number received in any RTP packet. Wrap-around is
  // possible.
  uint32_t extended_high_sequence_number = 0;

  // An estimate of the recent variance in RTP packet arrival times.
  RtpTimeDelta jitter;

  // The last Status Report received.
  StatusReportId last_status_report_id;

  // The delay between when the peer received the most-recent Status Report and
  // when this report was sent. The timebase is 65536 ticks per second and,
  // because of the wire format, this value will always be in the range
  // [0,65536) seconds.
  using Delay = std::chrono::duration<int64_t, std::ratio<1, 65536>>;
  Delay delay_since_last_report{};

  // Serializes this report block in the first |kRtcpReportBlockSize| bytes of
  // the given |buffer|.
  void Serialize(absl::Span<uint8_t> buffer) const;

  // Scans the wire-format report blocks in |buffer|, searching for one with the
  // matching |recipient_ssrc| and, if found, returns the parse result. Returns
  // nullopt if the data is corrupt or no report block with the matching SSRC
  // was found.
  static absl::optional<RtcpReportBlock> ParseOne(
      absl::Span<const uint8_t> buffer,
      int num_report_blocks,
      Ssrc recipient_ssrc);
};

struct RtcpSenderReport {
  RtcpSenderReport();
  ~RtcpSenderReport();

  // The point-in-time at which this report was sent, in terms of local system
  // time.
  platform::Clock::time_point reference_time{};

  // The media timestamp corresponding to the reference time.
  RtpTimeTicks rtp_timestamp;

  // The total number of RTP packets transmitted since the start of the session
  // (wrap-around is possible).
  uint32_t send_packet_count = 0;

  // The total number of payload bytes transmitted in RTP packets since the
  // start of the session (wrap-around is possible).
  uint32_t send_octet_count = 0;

  // The report block, if present. While the RTCP spec allows for zero or
  // multiple reports, Cast Streaming only uses zero or one.
  absl::optional<RtcpReportBlock> report_block;
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_RTCP_COMMON_H_
