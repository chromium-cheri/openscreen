// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_FRAME_COLLECTOR_H_
#define STREAMING_CAST_FRAME_COLLECTOR_H_

#include <vector>

#include "streaming/cast/frame_crypto.h"
#include "streaming/cast/frame_id.h"
#include "streaming/cast/rtcp_common.h"
#include "streaming/cast/rtp_packet_parser.h"

namespace openscreen {
namespace cast_streaming {

// Used by a Receiver to collect the parts of a frame, track what is
// missing/complete, and assemble a complete frame.
class FrameCollector {
 public:
  FrameCollector();
  ~FrameCollector();

  // Sets the ID of the current frame being collected. This must be called after
  // each Reset(), and before any of the other methods.
  void set_frame_id(FrameId frame_id) { frame_.frame_id = frame_id; }

  // Examine the parsed packet, representing part of the whole frame, and
  // collect any data/metadata from it that helps complete the frame. Returns
  // false if the |part| contained invalid data.
  [[nodiscard]] bool CollectPart(const RtpPacketParser::ParseResult& part);

  // Returns true if the frame data collection is complete and the frame can be
  // assembled.
  bool has_all_parts() const { return num_missing_packets_ == 0; }

  // Appends zero or more elements to |nacks| representing which packets are not
  // yet collected. If all packets for the frame are missing, this appends a
  // single element containing the special kAllPacketsLost packet ID. Otherwise,
  // one element is appended for each missing packet, in increasing order of
  // packet ID.
  void AppendMissingPackets(std::vector<PacketNack>* nacks) const;

  // Performs any last-minute data shuffling if needed and returns a read-only
  // reference to the frame. The caller should reset the FrameCollector (see
  // Reset() below) to free-up memory once it has finished reading from the
  // returned frame.
  //
  // Precondition: has_all_parts() must return true before this method can be
  // called.
  const EncryptedFrame& PeekAtAssembledFrame();

  // Resets the FrameCollector back to its initial state, freeing-up memory.
  void Reset();

 private:
  struct DataChunkSpan {
    int offset;
    int size;

    // Integer constant representing a missing chunk of frame data.
    static constexpr int kMissingOffset = -1;

    constexpr bool operator<(const DataChunkSpan& other) const {
      return offset < other.offset;
    }
  };

  // Storage for frame metadata and data, both while the frame is being
  // collected and once it has been assembled for external read-only use.
  // |frame_.data| is set to non-null once the frame has been assembled.
  EncryptedFrame frame_;

  // The number of packets needed to complete the frame, or the maximum int if
  // this is not yet known.
  int num_missing_packets_;

  // Location of frame data chunks that have been appended to |frame_.data|, as
  // a map of FramePacketId → {offset,size}.
  //
  // TODO(issues/56): Replace this with a scheme where packet buffers are
  // taken-in and owned, once the new UdpSocket/UdpPacket APIs are plumbed into
  // upstream modules.
  std::vector<DataChunkSpan> chunks_;
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_FRAME_COLLECTOR_H_
