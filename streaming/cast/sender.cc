// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "streaming/cast/sender.h"

#include <algorithm>
#include <ratio>

#include "platform/api/logging.h"
#include "util/std_util.h"

using ClockDuration = openscreen::platform::Clock::duration;
using std::chrono::duration_cast;

namespace openscreen {
namespace cast_streaming {

namespace {

template <typename Container>
bool HasLargestElementAtBack(const Container& c) {
  return !c.empty() && AreElementsSortedAndUnique(c);
}

}  // namespace

Sender::Sender(Environment* environment,
               SenderTransport* transport,
               Ssrc sender_ssrc,
               Ssrc receiver_ssrc,
               RtpPayloadType rtp_payload_type,
               int rtp_timebase,
               const std::array<uint8_t, 16>& aes_key,
               const std::array<uint8_t, 16>& cast_iv_mask)
    : environment_(environment),
      transport_(transport),
      // TODO(jophba): Is now a good time?
      rtcp_session_(sender_ssrc, receiver_ssrc, platform::Clock::now()),
      rtcp_parser_(&rtcp_session_, this),
      sender_report_builder_(&rtcp_session_),
      rtp_packetizer_(rtp_payload_type,
                      sender_ssrc,
                      transport_->max_packet_size()),
      rtp_timebase_(rtp_timebase),
      crypto_(aes_key, cast_iv_mask) {
  OSP_DCHECK(environment_);
  OSP_DCHECK(transport_);
  transport_->RegisterClient(ssrc(), this);
  transport_->RequestRtcpSend(ssrc());
}

Sender::~Sender() {
  transport_->DeregisterClient(ssrc());
}

void Sender::SetFrameCancelObserver(FrameCancelObserver* observer) {
  frame_cancel_observer_ = observer;
}

int Sender::GetInFlightFrameCount() const {
  return last_enqueued_frame_id_ - checkpoint_frame_id_;
}

ClockDuration Sender::GetInFlightMediaDuration(
    platform::Clock::time_point next_frame_reference_time) const {
  if (GetInFlightFrameCount() == 0) {
    return ClockDuration::zero();
  }
  const PendingFrameSlot& slot_for_oldest =
      pending_frames_[ToSlotIndex(checkpoint_frame_id_ + 1)];
  OSP_DCHECK(slot_for_oldest.frame);
  return next_frame_reference_time - slot_for_oldest.frame->reference_time;
}

ClockDuration Sender::GetMaxInFlightMediaDuration() const {
  // The total amount allowed in-flight media should equal the amount that fits
  // within the entire playout delay window, plus the amount of time it takes to
  // receive an ACK from the Receiver. Anything more would be overflow. In fact,
  // a little less *might* already be considered overflow.
  //
  // TODO(miu): Revisit this (it was carried-over from the Chromium
  // implementation).
  return target_playout_delay_ + (round_trip_time_ / 2);
}

bool Sender::NeedsKeyFrame() const {
  return last_enqueued_key_frame_id_ <= picture_lost_at_frame_id_;
}

FrameId Sender::GetNextFrameId() const {
  return last_enqueued_frame_id_ + 1;
}

Sender::EnqueueFrameResult Sender::EnqueueFrame(const EncodedFrame& frame) {
  OSP_DCHECK_EQ(frame.frame_id, GetNextFrameId());

  // Check whether enqueuing the frame would exceed the current maximum media
  // duration limit.
  if (GetInFlightMediaDuration(frame.reference_time) >
      GetMaxInFlightMediaDuration()) {
    return MAX_DURATION_IN_FLIGHT;
  }

  // Check that the slot for the frame isn't already occupied.
  PendingFrameSlot& slot = pending_frames_[ToSlotIndex(frame.frame_id)];
  if (slot.frame) {
    // Each successive frame should have a FrameId value one higher than the
    // last. Thus, if the slot is occupied, then all slots are known to be
    // occupied. In other words, the "max unacked frames" limit has been
    // reached.
    return MAX_FRAMES_IN_FLIGHT;
  }

  // Encrypt the frame and initialize the slot tracking its sending.
  slot.frame = crypto_.Encrypt(frame);
  const int packet_count = rtp_packetizer_.ComputeNumberOfPackets(*slot.frame);
  if (packet_count <= 0) {
    slot.frame = absl::nullopt;
    return PAYLOAD_TOO_LARGE;
  }
  slot.packet_send_flags.Resize(packet_count, YetAnotherBitVector::SET);
  slot.packet_sent_times.assign(packet_count, SenderTransport::kNever);

  // Officially record the "enqueue."
  last_enqueued_frame_id_ = slot.frame->frame_id;
  if (slot.frame->dependency == EncodedFrame::KEY) {
    last_enqueued_key_frame_id_ = slot.frame->frame_id;
  }

  // Update the target playout delay, if necessary.
  if (slot.frame->new_playout_delay >
      decltype(slot.frame->new_playout_delay)::zero()) {
    target_playout_delay_ = slot.frame->new_playout_delay;
    playout_delay_change_frame_id_ = slot.frame->frame_id;
  }

  // Update the lip-sync information for the next Sender Report.
  pending_sender_report_.reference_time = slot.frame->reference_time;
  pending_sender_report_.rtp_timestamp = slot.frame->rtp_timestamp;

  // If the round trip time hasn't been computed yet, aggressively send out a
  // Sender Report which contains the required lip-sync information for playout.
  // When the round trip time is known, that means a Receiver Report has been
  // successfully processed on this end; and, working backwards, that the
  // Receiver generated a report from its end; and, it could have only done that
  // after having successfully processed a Sender Report generated from this
  // end.
  if (round_trip_time_ == ClockDuration::zero()) {
    transport_->RequestRtcpSend(ssrc());
  }

  // Record the "flight" plan for the payload data to be transmitted. This is
  // used by the SenderTransport's bandwidth availability estimator. The start
  // time is "now" and the end time depends on working backwards from the
  // playout time: Playout occurs at the fixed |target_playout_delay_| (since
  // capture time). Before that, the Receiver needs some time to decode/process
  // a fully-received frame. Before that, the last packet must have departed the
  // Sender.
  const auto receiver_processing_time = target_playout_delay_ / 8;
  const auto network_travel_time = round_trip_time_ / 2;
  const platform::Clock::time_point departure_deadline =
      slot.frame->reference_time + target_playout_delay_ -
      receiver_processing_time - network_travel_time;
  transport_->RecordFlightPlan(slot.frame->data.size(), now(),
                               departure_deadline);

  // Re-activate RTP sending if it was suspended.
  transport_->RequestRtpSend(ssrc());

  return OK;
}

void Sender::OnReceivedRtcpPacket(absl::Span<const uint8_t> packet) {
  // This will invoke zero or more of the OnReceiverXYZ() methods in the same
  // call stack:
  rtcp_parser_.Parse(packet, last_enqueued_frame_id_);
}

ClockDuration Sender::GetCurrentRoundTripTime() {
  return round_trip_time_;
}

absl::Span<uint8_t> Sender::GetRtcpPacketForImmediateSend(
    absl::Span<uint8_t> buffer) {
  if (pending_sender_report_.reference_time == platform::Clock::time_point()) {
    // Cannot send a report if a frame has never been enqueued.
    return buffer.subspan(0, 0);
  }

  // Advance the reference time in the report to the RTCP packet send time
  // (i.e., "right now"), and adjust the corresponding RTP timestamp to match.
  RtcpSenderReport sender_report = pending_sender_report_;
  sender_report.reference_time = now();
  sender_report.rtp_timestamp += RtpTimeDelta::FromDuration(
      sender_report.reference_time - pending_sender_report_.reference_time,
      rtp_timebase_);

  // Build the packet containing the Sender Report, and weakly track the
  // StatusReportId that refers to the report. This information is used later,
  // in OnReceiverReport(), to compute the current network round trip time.
  const std::pair<absl::Span<uint8_t>, StatusReportId> result =
      sender_report_builder_.BuildPacket(sender_report, buffer);
  if (!result.first.empty()) {
    report_send_times_tail_->id = result.second;
    report_send_times_tail_->when = sender_report.reference_time;
    ++report_send_times_tail_;
    if (report_send_times_tail_ == report_send_times_.end()) {
      report_send_times_tail_ = report_send_times_.begin();
    }
  }

  return result.first;
}

absl::Span<uint8_t> Sender::GetRtpPacketForImmediateSend(
    absl::Span<uint8_t> buffer) {
  const auto slot_and_packet_id = ChooseNextRtpPacketForImmediateSend();

  // If a choice was not made, return "empty" to signal to the transport that
  // there is nothing to send. In this case, the transport may suspend RTP
  // sending until this Sender explicitly resumes it.
  PendingFrameSlot* const slot = slot_and_packet_id.first;
  if (!slot) {
    return buffer.subspan(0, 0);
  }

  // Generate the packet and update per-packet tracking info.
  const FramePacketId packet_id = slot_and_packet_id.second;
  const absl::Span<uint8_t> result =
      rtp_packetizer_.GeneratePacket(*slot->frame, packet_id, buffer);
  OSP_DCHECK(!result.empty());
  slot->packet_send_flags.Clear(packet_id);
  slot->packet_sent_times[packet_id] = now();

  // Update statistics for the next Sender Report.
  ++pending_sender_report_.send_packet_count;
  // Technically, the octet count should not include the RTP header, according
  // to RFC3550. However, no known Cast Streaming Receiver implementations use
  // this and it would add a bit of code complexity to track this precisely.
  // So, just shove-in a close approximation to the truth here.
  pending_sender_report_.send_octet_count += result.size();

  return result;
}

platform::Clock::time_point Sender::GetRtpResumeTime() {
  // Resume sending soon if the Receiver isn't aware of all the frames in
  // existence and may need a Kickstart packet.
  const auto slot_and_packet_id = ChooseKickstartPacket();
  if (slot_and_packet_id.first) {
    auto from =
        slot_and_packet_id.first->packet_sent_times[slot_and_packet_id.second];
    if (from == SenderTransport::kNever) {
      return now();
    }
    return from + kickstart_interval();
  }

  // Otherwise, tell the transport to continue to suspend, since this Sender
  // will explicity resume it at the appropriate time.
  return SenderTransport::kNever;
}

void Sender::OnReceiverReferenceTimeAdvanced(
    platform::Clock::time_point reference_time) {
  // Not used.
}

void Sender::OnReceiverReport(const RtcpReportBlock& receiver_report) {
  // The Receiver Report is referencing a previous Sender Report. Search the
  // cache for when that report was sent.
  const platform::Clock::time_point* when_sender_report_was_sent = nullptr;
  for (const SenderReportTiming& entry : report_send_times_) {
    if (entry.id == receiver_report.last_status_report_id) {
      when_sender_report_was_sent = &entry.when;
      break;
    }
  }
  if (!when_sender_report_was_sent) {
    return;  // Not found. It's likely the Receiver Report is too old.
  }

  // Calculate the round trip time: This is the time elapsed since the Sender
  // Report was sent, minus the time the Receiver did other stuff before sending
  // the Receiver Report back.
  //
  // If the round trip time seems to be less than or equal to zero, assume clock
  // imprecision by one or both peers caused a bad value to be calculated. The
  // true value is likely very close to zero (i.e., this is ideal network
  // behavior); and so just represent this as 75 µs, an optimistic
  // wired-Ethernet ping time.
  constexpr auto kNearZeroRoundTripTime =
      duration_cast<ClockDuration>(std::chrono::microseconds(75));
  static_assert(kNearZeroRoundTripTime > ClockDuration::zero(),
                "More precision needed!");
  const ClockDuration measurement = std::max(
      (now() - *when_sender_report_was_sent) -
          duration_cast<ClockDuration>(receiver_report.delay_since_last_report),
      kNearZeroRoundTripTime);

  // Measurements will typically have high variance. Use a simple smoothing
  // filter to track a short-term average that changes less drastically.
  if (round_trip_time_ == ClockDuration::zero()) {
    round_trip_time_ = measurement;
  } else {
    constexpr int kInertia = 7;
    round_trip_time_ =
        (kInertia * round_trip_time_ + measurement) / (kInertia + 1);
  }
}

void Sender::OnReceiverIndicatesPictureLoss() {
  // The Receiver will continue the PLI notifications until it has received a
  // key frame. Thus, if a key frame is already in-flight, don't make a state
  // change that would cause this Sender to force another expensive key frame.
  if (checkpoint_frame_id_ < last_enqueued_key_frame_id_) {
    return;
  }

  picture_lost_at_frame_id_ = checkpoint_frame_id_;

  // Note: It may seem that all pending frames should be canceled until
  // EnqueueFrame() is called with a key frame. However:
  //
  //   1. The Receiver should still be the main authority on what frames/packets
  //      are being ACK'ed and NACK'ed.
  //
  //   2. It may be desirable for the Receiver to be "limping along" in the
  //      meantime. For example, video may be corrupted but mostly watchable,
  //      and so it's best for the Sender to continue sending the non-key frames
  //      until the Receiver indicates otherwise.
}

void Sender::OnReceiverCheckpoint(FrameId frame_id,
                                  std::chrono::milliseconds playout_delay) {
  const platform::Clock::time_point feedback_time_minus_rtt =
      now() - round_trip_time_;
  transport_->RecordFeedback(0, feedback_time_minus_rtt);
  while (checkpoint_frame_id_ < frame_id) {
    ++checkpoint_frame_id_;
    CancelPendingFrame(checkpoint_frame_id_, feedback_time_minus_rtt);
  }
  latest_expected_frame_id_ = std::max(latest_expected_frame_id_, frame_id);

  if (playout_delay != target_playout_delay_ &&
      frame_id >= playout_delay_change_frame_id_) {
    OSP_LOG_WARN << "Sender's target playout delay ("
                 << target_playout_delay_.count()
                 << " ms) disagrees with the Receiver's ("
                 << playout_delay.count() << " ms)";
  }
}

void Sender::OnReceiverHasFrames(std::vector<FrameId> acks) {
  OSP_DCHECK(!acks.empty() && AreElementsSortedAndUnique(acks));
  const platform::Clock::time_point feedback_time_minus_rtt =
      now() - round_trip_time_;
  transport_->RecordFeedback(0, feedback_time_minus_rtt);
  for (FrameId id : acks) {
    CancelPendingFrame(id, feedback_time_minus_rtt);
  }
  latest_expected_frame_id_ = std::max(latest_expected_frame_id_, acks.back());
}

void Sender::OnReceiverIsMissingPackets(std::vector<PacketNack> nacks) {
  OSP_DCHECK(!nacks.empty() && AreElementsSortedAndUnique(nacks));

  // This is a point-in-time threshold that indicates whether each NACK will
  // trigger a packet retransmit. The threshold is based on the network round
  // trip time because a Receiver's NACK may have been issued while the needed
  // packet was in-flight from the Sender. In such cases, the Receiver's NACK is
  // likely stale and this Sender should not redundantly re-transmit the packet
  // again.
  const auto too_recent_a_send_time = now() - round_trip_time_;

  // Iterate over all the NACKs...
  bool need_to_send = false;
  for (auto nack_it = nacks.begin(); nack_it != nacks.end();) {
    // Find the slot associated with the NACK's frame ID.
    const FrameId frame_id = nack_it->frame_id;
    PendingFrameSlot* slot = nullptr;
    if (frame_id <= last_enqueued_frame_id_) {
      PendingFrameSlot& candidate_slot = pending_frames_[ToSlotIndex(frame_id)];
      if (candidate_slot.is_active_for_frame(frame_id)) {
        slot = &candidate_slot;
      }
    }

    // If no slot was found (i.e., the NACK is invalid) for the frame, skip-over
    // all other NACKs for the same frame. While it seems to be a bug that the
    // Receiver would attempt to NACK a frame that does not yet exist, this can
    // happen in rare cases where the parser incorrectly expands the truncated
    // frame ID field found in some old packet data.
    if (!slot) {
      OSP_LOG_WARN << "Ignoring invalid NACK(s) for frame " << frame_id;
      for (++nack_it; nack_it != nacks.end() && nack_it->frame_id == frame_id;
           ++nack_it)
        ;
      continue;
    }

    // Process all the NACKs for the same frame: Set the send flag on any
    // NACK'ed packets that have not been sent recently.
    do {
      FramePacketId packet_id = nack_it->packet_id;
      if (packet_id == kAllPacketsLost) {
        // Special case: The Receiver is NACK'ing all the packets in this frame.
        for (packet_id = 0; packet_id < slot->packet_sent_times.size();
             ++packet_id) {
          if (slot->packet_sent_times[packet_id] < too_recent_a_send_time) {
            slot->packet_send_flags.Set(packet_id);
            need_to_send = true;
          }
        }
      } else if (packet_id < slot->packet_sent_times.size()) {
        // Typical case: The Receiver is NACK'ing a single, specific packet.
        if (slot->packet_sent_times[packet_id] < too_recent_a_send_time) {
          slot->packet_send_flags.Set(packet_id);
          need_to_send = true;
        }
      } else {
        OSP_LOG_WARN << "Ingoring NACK for packet that doesn't exist in frame "
                     << frame_id << ": " << static_cast<int>(packet_id);
      }
      ++nack_it;
    } while (nack_it != nacks.end() && nack_it->frame_id == frame_id);
  }

  latest_expected_frame_id_ =
      std::max(latest_expected_frame_id_, nacks.back().frame_id);

  if (need_to_send) {
    transport_->RequestRtpSend(ssrc());
  }
}

std::pair<Sender::PendingFrameSlot*, FramePacketId>
Sender::ChooseNextRtpPacketForImmediateSend() {
  // Search for a packet that has been flagged as "need to send" and return the
  // first one found.
  for (FrameId frame_id = checkpoint_frame_id_ + 1;
       frame_id < last_enqueued_frame_id_; ++frame_id) {
    PendingFrameSlot& slot = pending_frames_[ToSlotIndex(frame_id)];
    if (!slot.is_active_for_frame(frame_id)) {
      continue;  // Frame was already canceled (e.g., ACK'ed by the Receiver).
    }
    const FramePacketId packet_id = slot.packet_send_flags.FindFirstSet();
    if (packet_id < slot.packet_send_flags.size()) {
      return {&slot, packet_id};
    }
  }

  // If no packets need sending (i.e., all packets have been sent at least once
  // and do not need to be re-sent yet), check whether a Kickstart packet should
  // be sent. It's possible that there has been complete packet loss of some
  // frames, and the Receiver may not be aware of the existance of the latest
  // frame(s). Kickstarting is the only way the Receiver can discover the newer
  // frames it doesn't know about.
  return ChooseKickstartPacket();
}

std::pair<Sender::PendingFrameSlot*, FramePacketId>
Sender::ChooseKickstartPacket() {
  if (latest_expected_frame_id_ >= last_enqueued_frame_id_) {
    return {nullptr, FramePacketId{0}};
  }

  // The Kickstart packet is always in the last-enqueued frame, so that the
  // Receiver will know about every frame the Sender has. However, which packet
  // should be chosen? Any would do, since all packets contain the frame's total
  // packet count. For historical reasons, all sender implementations have
  // always just sent the last packet; and so that tradition is continued here.
  PendingFrameSlot& slot =
      pending_frames_[ToSlotIndex(last_enqueued_frame_id_)];
  OSP_DCHECK(slot.is_active_for_frame(last_enqueued_frame_id_));
  const auto last_packet_id =
      static_cast<FramePacketId>(slot.packet_send_flags.size() - 1);
  return {&slot, last_packet_id};
}

void Sender::CancelPendingFrame(
    FrameId frame_id,
    platform::Clock::time_point feedback_time_minus_rtt) {
  // TODO: Tracing, like OSP_VLOG << __func__ << '(' << frame_id << ')';
  PendingFrameSlot& slot = pending_frames_[ToSlotIndex(frame_id)];
  if (slot.is_active_for_frame(frame_id)) {
    transport_->RecordFeedback(slot.frame->data.size(),
                               feedback_time_minus_rtt);
    slot.frame.reset();
    if (frame_cancel_observer_) {
      frame_cancel_observer_->OnFrameCanceled(frame_id);
    }
  }
}

// static
int Sender::ToSlotIndex(FrameId frame_id) {
  constexpr int slot_count = std::tuple_size<decltype(pending_frames_)>();
  return (frame_id - FrameId::first()) % slot_count;
}

Sender::FrameCancelObserver::~FrameCancelObserver() = default;

Sender::PendingFrameSlot::PendingFrameSlot() = default;
Sender::PendingFrameSlot::~PendingFrameSlot() = default;

}  // namespace cast_streaming
}  // namespace openscreen
