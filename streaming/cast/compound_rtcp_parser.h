// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_COMPOUND_RTCP_PARSER_H_
#define STREAMING_CAST_COMPOUND_RTCP_PARSER_H_

#include <chrono>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "streaming/cast/frame_id.h"
#include "streaming/cast/rtcp_common.h"
#include "streaming/cast/rtp_defines.h"

namespace openscreen {
namespace cast_streaming {

class RtcpSession;

// Parses compound RTCP packets from a Receiver, invoking client callbacks when
// information of interest to a Sender (in the current process) is encountered.
class CompoundRtcpParser {
 public:
  // Callback interface used while parsing RTCP packets of interest to a Sender.
  // The implementation must take into account:
  //
  //   1. Some/All of the data could be stale, as it only reflects the state of
  //      the Receiver at the time the packet was generated. A significant
  //      amount of time may have passed, depending on how long it took the
  //      packet to reach this local instance over the network.
  //   2. The data shouldn't necessarily be trusted blindly: Some may be
  //      inconsistent (e.g., the same frame being ACKed and NACKed; or a frame
  //      that has not been sent yet is being NACKed). While that would indicate
  //      a badly-behaving Receiver, the Sender should be robust to such things.
  class Client {
   public:
    Client();
    virtual ~Client();

    // Called when a Receiver Reference Time Report has been parsed from a
    // non-stale packet.
    virtual void OnReceiverReferenceTimeAdvanced(
        platform::Clock::time_point reference_time);

    // Called when a Receiver Report with a Report Block has been parsed.
    virtual void OnReceiverReport(const RtcpReportBlock& receiver_report);

    // Called when the Receiver has encountered an unrecoverable error in
    // decoding the data. The Sender should provide a key frame as soon as
    // possible.
    virtual void OnReceiverLostPicture();

    // Called when the Receiver indicates that all of the packets for all frames
    // up to and including |frame_id| have been successfully received (or
    // otherwise do not need to be re-transmitted). The |playout_delay| is the
    // Receiver's current end-to-end target playout delay setting, which should
    // reflect any changes the Sender has made by using the "Cast Adaptive
    // Latency Extension" in RTP packets.
    virtual void OnReceiverCheckpoint(FrameId frame_id,
                                      std::chrono::milliseconds playout_delay);

    // Called to indicate the Receiver has successfully received all of the
    // packets for each of the given |frame_ids|.
    virtual void OnReceiverHasFrames(absl::Span<const FrameId> frame_ids);

    // Called to indicate the Receiver is missing all of the packets for each of
    // the given |frame_ids|.
    virtual void OnReceiverIsMissingFrames(absl::Span<const FrameId> frame_ids);

    // Called to indicate the Receiver is missing certain specific |packets| for
    // certain |frame_ids|.
    virtual void OnReceiverIsMissingPackets(
        absl::Span<const std::pair<FrameId, FramePacketId>> packet_ids);
  };

  // |session| and |client| must be non-null and must outlive the
  // CompoundRtcpParser instance.
  CompoundRtcpParser(RtcpSession* session, Client* client);
  ~CompoundRtcpParser();

  // Gets/Sets the ID of the latest frame whose first packet is about to be
  // sent. This is the maximum-valued FrameId that could possibly be
  // ACKnowledged by the Receiver in the next RTCP packet. This is needed for
  // expanding truncated frame IDs during Parse().
  FrameId max_feedback_frame_id() const { return max_feedback_frame_id_; }
  void SetMaxFeedbackFrameId(FrameId frame_id);

  // Parses the packet, invoking the Client callback methods when appropriate.
  // Returns true if the |packet| was well-formed, or false if it was corrupt.
  // Note that none of the Client callback methods will be invoked until a
  // packet is known to be well-formed.
  bool Parse(absl::Span<const uint8_t> packet);

 private:
  // These return true if the input was well-formed, and false if it was
  // invalid/corrupt. The true/false value does NOT indicate whether the data
  // contained within was ignored.
  bool ParseReceiverReport(absl::Span<const uint8_t> in, int num_report_blocks);
  bool ParseReportBlocks(absl::Span<const uint8_t> in,
                         int num_report_blocks,
                         absl::optional<RtcpReportBlock>* out);
  bool ParseFeedback(absl::Span<const uint8_t> in);
  bool ParseExtendedReports(absl::Span<const uint8_t> in);
  bool ParsePictureLossIndicator(absl::Span<const uint8_t> in);

  RtcpSession* const session_;
  Client* const client_;

  // The following temporarily store the results from the various ParseXYZ()
  // methods. This is to validate that the entire input parses successfully
  // before any of the Client callbacks are invoked.
  absl::optional<platform::Clock::time_point> receiver_reference_time_;
  absl::optional<RtcpReportBlock> receiver_report_;
  absl::optional<std::pair<FrameId, std::chrono::milliseconds>> checkpoint_;
  std::vector<FrameId> received_frames_;
  std::vector<FrameId> missing_frames_;
  std::vector<std::pair<FrameId, FramePacketId>> missing_packets_;
  bool picture_loss_indicator_ = false;

  // The maximum possible re-expanded frame ID value to expect within the RTCP
  // feedback reports being parsed.
  FrameId max_feedback_frame_id_;

  // Tracks the latest timestamp seen from any Receiver Reference Time Report,
  // and uses this to ignore stale RTCP packets that arrived out-of-order and/or
  // late from the network.
  absl::optional<platform::Clock::time_point> latest_receiver_timestamp_;
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_COMPOUND_RTCP_PARSER_H_
