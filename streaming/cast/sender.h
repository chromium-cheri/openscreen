// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_SENDER_H_
#define STREAMING_CAST_SENDER_H_

#include <stdint.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "platform/api/time.h"
#include "streaming/cast/compound_rtcp_parser.h"
#include "streaming/cast/constants.h"
#include "streaming/cast/frame_crypto.h"
#include "streaming/cast/rtp_defines.h"
#include "streaming/cast/rtp_packetizer.h"
#include "streaming/cast/sender_report_builder.h"
#include "streaming/cast/sender_transport.h"

namespace openscreen {
namespace cast_streaming {

class Environment;

class Sender final : public SenderTransport::Client,
                     public CompoundRtcpParser::Client {
 public:
  // Interface for receiving notifications that a frame was canceled. "Canceled"
  // means that the Receiver has either acknowledged successful receipt of the
  // frame or has decided to skip over it.
  class FrameCancelObserver {
   public:
    virtual ~FrameCancelObserver();
    virtual void OnFrameCanceled(FrameId frame_id) = 0;
  };

  // Result codes for EnqueueFrame().
  enum EnqueueFrameResult {
    OK,                      // Frame has been queued for sending.
    PAYLOAD_TOO_LARGE,       // Frame's payload was too large.
    MAX_FRAMES_IN_FLIGHT,    // Too many frames are already in-flight.
    MAX_DURATION_IN_FLIGHT,  // Too-large a media duration is in-flight.
  };

  // The constructor is generally invoked by SenderSession::AddServer(), which
  // provides the necessary injected dependencies, though unit tests might
  // invoke this directly. All arguments are documented there.
  Sender(Environment* environment,
         SenderTransport* transport,
         Ssrc sender_ssrc,
         Ssrc receiver_ssrc,
         RtpPayloadType rtp_payload_type,
         int rtp_timebase,
         const std::array<uint8_t, 16>& aes_key,
         const std::array<uint8_t, 16>& cast_iv_mask);

  virtual ~Sender() final;

  Ssrc ssrc() const { return rtcp_session_.sender_ssrc(); }

  // Sets an observer for receiving frame cancel notifications. Call with
  // nullptr to stop observing.
  void SetFrameCancelObserver(FrameCancelObserver* observer);

  // Returns the number of frames currently in-flight. The maximum, per the
  // design limit, is kMaxUnackedFrames.
  int GetInFlightFrameCount() const;

  // Returns the duration of the media covered by the frames currently
  // in-flight, assuming the next frame to be enqueued will have the given
  // reference time.
  platform::Clock::duration GetInFlightMediaDuration(
      platform::Clock::time_point next_frame_reference_time) const;

  // Return the maximum acceptable in-flight media duration, given current
  // end-to-end system conditions.
  platform::Clock::duration GetMaxInFlightMediaDuration() const;

  // Returns true if the receiver requires a key frame.
  bool NeedsKeyFrame() const;

  // Returns the next FrameId, the one after the frame enqueued by the last call
  // to EnqueueFrame().
  FrameId GetNextFrameId() const;

  // Enqueues the given |frame| for sending as soon as possible. Returns OK if
  // the frame is accepted (and some time later the FrameCancelObserver will be
  // notified once it is no longer in-flight).
  [[nodiscard]] EnqueueFrameResult EnqueueFrame(const EncodedFrame& frame);

 private:
  // Tracking/Storage for frames that are ready-to-send, but not yet fully
  // received at the other end.
  struct PendingFrameSlot {
    // The frame to send, or nullopt if there is no frame in this slot.
    absl::optional<EncryptedFrame> frame;

    // Represents which packets need to be sent (elements are indexed by
    // FramePacketId). A set bit means a packet needs to be sent (or re-sent).
    YetAnotherBitVector packet_send_flags;

    // The time when each of the packets was last sent, or
    // SenderTransport::kNever if the packet has not been sent yet. Elements are
    // indexed by FramePacketId. This is used to avoid re-transmitting any given
    // packet too frequently.
    std::vector<platform::Clock::time_point> packet_sent_times;

    PendingFrameSlot();
    ~PendingFrameSlot();

    bool is_active_for_frame(FrameId frame_id) const {
      return frame && frame->frame_id == frame_id;
    }
  };

  // Tracking of when specific SenderReports were sent.
  struct SenderReportTiming {
    StatusReportId id{};
    platform::Clock::time_point when{};
  };

  platform::Clock::time_point now() const {
    return environment_->now_function()();
  }

  // The amount of time to wait between sending Kickstart packets.
  platform::Clock::duration kickstart_interval() const {
    return std::max(round_trip_time_,
                    std::chrono::duration_cast<platform::Clock::duration>(
                        kMinKickstartInterval));
  }

  // SenderTransport::Client implementation.
  void OnReceivedRtcpPacket(absl::Span<const uint8_t> packet) final;
  platform::Clock::duration GetCurrentRoundTripTime() final;
  absl::Span<uint8_t> GetRtcpPacketForImmediateSend(
      absl::Span<uint8_t> buffer) final;
  absl::Span<uint8_t> GetRtpPacketForImmediateSend(
      absl::Span<uint8_t> buffer) final;
  platform::Clock::time_point GetRtpResumeTime() final;

  // CompoundRtcpParser::Client implementation.
  void OnReceiverReferenceTimeAdvanced(
      platform::Clock::time_point reference_time) final;
  void OnReceiverReport(const RtcpReportBlock& receiver_report) final;
  void OnReceiverIndicatesPictureLoss() final;
  void OnReceiverCheckpoint(FrameId frame_id,
                            std::chrono::milliseconds playout_delay) final;
  void OnReceiverHasFrames(std::vector<FrameId> acks) final;
  void OnReceiverIsMissingPackets(std::vector<PacketNack> nacks) final;

  // Helper used by GetRtpPacketForImmediateSend() to choose which packet to
  // send. Returns {nullptr, 0} if nothing should be sent.
  std::pair<PendingFrameSlot*, FramePacketId>
  ChooseNextRtpPacketForImmediateSend();

  // Helper that returns the slot and packet ID that would be used to kick-start
  // the Receiver again, or {nullptr, 0} if kick-starting should not take place.
  // See discussion in the implementation of
  // ChooseNextRtpPacketForImmediateSend() for more details.
  std::pair<PendingFrameSlot*, FramePacketId> ChooseKickstartPacket();

  // Returns the point-in-time used to decide whether to re-transmit a packet:
  // If the packet was last sent before this point, it can be re-sent.
  platform::Clock::time_point ComputeResendTimePointThreshold() const;

  // Cancels the given frame when it is known to have been fully received, or
  // the Receiver has indicated it does not want it re-sent. This clears the
  // corresponding entry in |pending_frames_| and notifies the
  // FrameCancelObserver.
  void CancelPendingFrame(FrameId frame_id,
                          platform::Clock::time_point feedback_time_minus_rtt);

  static int ToSlotIndex(FrameId frame_id);

  Environment* const environment_;
  SenderTransport* const transport_;
  RtcpSession rtcp_session_;
  CompoundRtcpParser rtcp_parser_;
  SenderReportBuilder sender_report_builder_;
  RtpPacketizer rtp_packetizer_;
  const int rtp_timebase_;  // RTP timestamp ticks per second.
  FrameCrypto crypto_;

  // Ring buffer of pending frames, whose valid range is
  // [checkpoint_frame_id_+1,last_enqueued_frame_id_]. The frame having FrameId
  // x will always be slotted at position x % pending_frames_.size() (use
  // ToSlotIndex()).
  std::array<PendingFrameSlot, kMaxUnackedFrames> pending_frames_{};

  // The ID of the last frame enqueued.
  FrameId last_enqueued_frame_id_ = FrameId::first() - 1;

  // Indicates that all of the packets for all frames up to and including this
  // FrameId have been successfully received (or otherwise do not need to be
  // re-transmitted).
  FrameId checkpoint_frame_id_ = FrameId::first() - 1;

  // The ID of the latest frame the Receiver seems to be aware of.
  FrameId latest_expected_frame_id_ = FrameId::first() - 1;

  // The target playout delay for the last-enqueued frame. This is auto-updated
  // when a frame is enqueued that changes the delay.
  std::chrono::milliseconds target_playout_delay_ = kDefaultTargetPlayoutDelay;
  FrameId playout_delay_change_frame_id_ = FrameId::first();

  // The near-term average round trip time. This is updated with each sender
  // report → receiver report round trip. This is initially zero, indicating the
  // round trip time has not been measured yet.
  platform::Clock::duration round_trip_time_{0};

  // Maintain current stats in a Sender Report that is ready for send at any
  // time. This includes up-to-date lip-sync information, and packet and byte
  // count stats.
  RtcpSenderReport pending_sender_report_;

  // A circularly-populated array that caches when recent Sender Reports were
  // sent. This is used to compute the network round trip time, when processing
  // Receiver Reports later on.
  static constexpr int kSenderReportHistorySize = 8;
  std::array<SenderReportTiming, kSenderReportHistorySize> report_send_times_{};
  decltype(report_send_times_)::iterator report_send_times_tail_ =
      report_send_times_.begin();

  // These control whether the Sender knows it needs to sent a key frame to the
  // Receiver. When the Receiver provides a picture loss notification, the
  // current checkpoint frame ID is stored in |picture_lost_at_frame_id_|. Then,
  // while |last_enqueued_key_frame_id_| is less than or equal to
  // |picture_lost_at_frame_id_|, the Sender knows it still needs to send a key
  // frame to resolve the picture loss condition. In all other cases, the
  // Receiver is either in a good state or is in the process of receiving the
  // key frame that will make that happen.
  FrameId picture_lost_at_frame_id_ = FrameId::first() - 1;
  FrameId last_enqueued_key_frame_id_ = FrameId::first() - 1;

  // The observer set/cleared by SetFrameCancelObserver().
  FrameCancelObserver* frame_cancel_observer_ = nullptr;

  // The interval between packet retransmits for kickstarting purposes. See
  // comments in ChooseNextRtpPacketForImmediateSend() for further details.
  static constexpr std::chrono::milliseconds kMinKickstartInterval{20};
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_SENDER_H_
