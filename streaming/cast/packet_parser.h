// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_PACKET_PARSER_H_
#define STREAMING_CAST_PACKET_PARSER_H_

#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "streaming/cast/rtp_defines.h"
#include "streaming/cast/ssrc.h"

namespace openscreen {
namespace cast_streaming {

// Parses RTP and RTCP packets.
class PacketParser {
 public:
  // |local_ssrc| and |remote_ssrc| are used to ignore packets that are not
  // meant to be visible to this instance.
  PacketParser(Ssrc local_ssrc, Ssrc remote_ssrc);
  ~PacketParser();

  // Parses the packet. Returns true if |packet| was well-formed, and the other
  // methods can be called to retrieve the parsed data.
  bool Parse(absl::Span<const uint8_t> packet);

  // Accessors of results of parsing a RTP packet.
  bool has_rtp_header_and_payload() const { return !!rtp_header_; }
  const RtpCastHeader& rtp_header() const { return *rtp_header_; }
  // The Span of bytes containing the payload (a subspan of the last |packet|
  // parsed). The caller must ensure the pointer is still valid!
  absl::Span<const uint8_t> rtp_payload() const { return rtp_payload_; }

  // Returns true if the given |packet| seems to be a RTP or RTCP packet and was
  // sent from the peer with the given SSRC. This only performs a very quick,
  // incomplete parse to determine this; and it does not guarantee that a full
  // parse will later succeed. This is used to route packets to the components
  // associated with specific streams.
  static bool IsPacketFromPeer(absl::Span<const uint8_t> packet,
                               Ssrc peer_ssrc);

 private:
  // These return true if the input was well-formed, and false if it was
  // invalid/corrupt. The true/false value does NOT indicate whether the data
  // contained within was ignored.
  bool ParseRtp(absl::Span<const uint8_t> packet);
  bool ParseRtcp(absl::Span<const uint8_t> packet);  // TODO(miu).

  const Ssrc local_ssrc_;
  const Ssrc remote_ssrc_;

  // Contents of the most-recent parse attempt. These are only valid if the last
  // call to Parse() returned true.
  absl::optional<RtpCastHeader> rtp_header_;
  absl::Span<const uint8_t> rtp_payload_;

  // Tracks recently-parsed RTP timestamps so that the truncated values can be
  // re-expanded into full-form.
  RtpTimeTicks last_parsed_rtp_timestamp_;

  // The highest frame ID seen in any RTP packets so far. This is tracked so
  // that the truncated frame ID fields in RTP packets can be re-expanded into
  // full-form.
  FrameId highest_rtp_frame_id_;
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_RTCP_PARSER_H_
