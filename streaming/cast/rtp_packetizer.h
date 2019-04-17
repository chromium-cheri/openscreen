// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_RTP_PACKETIZER_H_
#define STREAMING_CAST_RTP_PACKETIZER_H_

#include <stdint.h>

#include "absl/types/span.h"
#include "streaming/cast/frame_crypto.h"
#include "streaming/cast/rtp_defines.h"
#include "streaming/cast/ssrc.h"

namespace openscreen {
namespace cast_streaming {

// Transforms an EncryptedFrame into one or more RTP packets for transmission.
// This is meant to be a long-lived instance to generate the RTP packets for all
// the frames in the same media stream (i.e., per-SSRC).
class RtpPacketizer {
 public:
  RtpPacketizer(RtpPayloadType payload_type, Ssrc ssrc);
  ~RtpPacketizer();

  // Wire-format one of the RTP packets for the given frame, which must only be
  // transmitted once. This method should be called in the same sequence that
  // packets will be transmitted (for the same stream/SSRC). This also means
  // that, if a packet needs to be re-transmitted, this method should be called
  // to generate it again. Returns the subspan of |buffer| that contains the
  // packet, or an empy Span if the buffer was not large enough (use
  // kMaxRtpPacketSize!).
  absl::Span<uint8_t> GeneratePacket(const EncryptedFrame& frame,
                                     PacketId packet_id,
                                     absl::Span<uint8_t> buffer);

  // Given |frame|, compute the total number of packets over which the whole
  // frame will be split-up.
  static int ComputeNumberOfPackets(const EncryptedFrame& frame);

 private:
  // The validated ctor RtpPayloadType arg, in wire-format form.
  const uint8_t payload_type_7bits_;

  const Ssrc ssrc_;

  // Incremented each time GeneratePacket() is called. Every packet, even those
  // re-transmitted, must have different sequence numbers (within wrap-around
  // concerns) per the RTP spec.
  uint16_t sequence_number_;
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_RTP_PACKETIZER_H_
