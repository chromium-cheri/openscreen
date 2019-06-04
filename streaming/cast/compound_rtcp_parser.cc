// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "streaming/cast/compound_rtcp_parser.h"

#include <algorithm>

#include "osp_base/std_util.h"
#include "platform/api/logging.h"
#include "streaming/cast/packet_util.h"
#include "streaming/cast/rtcp_session.h"

namespace openscreen {
namespace cast_streaming {

namespace {

// Canonicalizes the just-parsed list of packet-specific NACKs so that the
// CompoundRtcpParser::Client can make several simplifying assumptions when
// processing the results.
void ScrubMissingPacketVector(std::vector<PacketNack>* packets) {
  // First, sort all elements. The sort order is the normal lexicographical
  // ordering, with one exception: The special kAllPacketsLost packet_id value
  // should be treated as coming before all others. This special sort order
  // allows the filtering algorithm below to be simpler, and only require one
  // pass; and the final result will be the normal lexicographically-sorted
  // output the CompoundRtcpParser::Client expects.
  std::sort(packets->begin(), packets->end(),
            [](const PacketNack& a, const PacketNack& b) {
              static_assert(static_cast<FramePacketId>(kAllPacketsLost + 1) <
                                FramePacketId{0x0000 + 1},
                            "comparison requires integer wrap-around");
              return PacketNack{a.frame_id,
                                static_cast<FramePacketId>(a.packet_id + 1)} <
                     PacketNack{b.frame_id,
                                static_cast<FramePacketId>(b.packet_id + 1)};
            });

  // De-duplicate elements. Two possible cases:
  //
  //   1. Identical elements (same FrameId+FramePacketId).
  //   2. If there are any elements with kAllPacketsLost as the packet ID,
  //      prune-out all other elements having the same frame ID, as they are
  //      redundant.
  //
  // This is done by walking forwards over the sorted vector and deciding which
  // elements to keep. Those that are kept are stacked-up at the front of the
  // vector. After the "to-keep" pass, the vector is truncated to remove the
  // left-over garbage at the end.
  auto have_it = packets->begin();
  if (have_it != packets->end()) {
    auto kept_it = have_it;  // Always keep the first element.
    for (++have_it; have_it != packets->end(); ++have_it) {
      if (have_it->frame_id != kept_it->frame_id ||
          (kept_it->packet_id != kAllPacketsLost &&
           have_it->packet_id != kept_it->packet_id)) {  // Keep it.
        ++kept_it;
        *kept_it = *have_it;
      }
    }
    packets->erase(++kept_it, packets->end());
  }
}

}  // namespace

CompoundRtcpParser::CompoundRtcpParser(RtcpSession* session,
                                       CompoundRtcpParser::Client* client)
    : session_(session), client_(client) {
  OSP_DCHECK(session_);
  OSP_DCHECK(client_);
}

CompoundRtcpParser::~CompoundRtcpParser() = default;

void CompoundRtcpParser::SetMaxFeedbackFrameId(FrameId frame_id) {
  OSP_DCHECK_GE(frame_id, max_feedback_frame_id_);
  max_feedback_frame_id_ = frame_id;
}

bool CompoundRtcpParser::Parse(absl::Span<const uint8_t> buffer) {
  // Reset.
  receiver_reference_time_ = absl::nullopt;
  receiver_report_ = absl::nullopt;
  checkpoint_ = absl::nullopt;
  received_frames_.clear();
  missing_packets_.clear();
  picture_loss_indicator_ = false;

  // The data contained in |buffer| can be a "compound packet," which means that
  // it can be the concatenation of multiple RTCP packets. The loop here
  // processes each one-by-one.
  while (!buffer.empty()) {
    const auto header = RtcpCommonHeader::Parse(buffer);
    if (!header) {
      return false;
    }
    buffer.remove_prefix(kRtcpCommonHeaderSize);
    if (static_cast<int>(buffer.size()) < header->payload_size) {
      return false;
    }
    const absl::Span<const uint8_t> chunk =
        buffer.subspan(0, header->payload_size);
    buffer.remove_prefix(header->payload_size);

    switch (header->packet_type) {
      case RtcpPacketType::kReceiverReport:
        if (!ParseReceiverReport(chunk, header->with.report_count)) {
          return false;
        }
        break;

      case RtcpPacketType::kPayloadSpecific:
        switch (header->with.subtype) {
          case RtcpSubtype::kPictureLossIndicator:
            if (!ParsePictureLossIndicator(chunk)) {
              return false;
            }
            break;
          case RtcpSubtype::kFeedback:
            if (!ParseFeedback(chunk)) {
              return false;
            }
            break;
          default:
            // Ignore: Unimplemented or not part of the Cast Streaming spec.
            break;
        }
        break;

      case RtcpPacketType::kExtendedReports:
        if (!ParseExtendedReports(chunk)) {
          return false;
        }
        break;

      default:
        // Ignored, unimplemented or not part of the Cast Streaming spec.
        break;
    }
  }

  // A well-behaved Cast Streaming Receiver will always include a reference time
  // report. This essentially "timestamps" the RTCP packets just parsed.
  // However, the spec does not explicitly require this be included. When it is
  // present, improve the stability of the system by ignoring stale/out-of-order
  // RTCP packets.
  if (receiver_reference_time_) {
    if (latest_receiver_timestamp_ &&
        *receiver_reference_time_ < *latest_receiver_timestamp_) {
      return true;
    }
    latest_receiver_timestamp_ = *receiver_reference_time_;
    client_->OnReceiverReferenceTimeAdvanced(*latest_receiver_timestamp_);
  }

  // At this point, the packet is known to be well-formed. Dispatch events of
  // interest to the Client.
  if (receiver_report_) {
    client_->OnReceiverReport(*receiver_report_);
  }
  if (checkpoint_) {
    client_->OnReceiverCheckpoint(checkpoint_->first, checkpoint_->second);
  }
  if (!received_frames_.empty()) {
    OSP_DCHECK(AreElementsSortedAndUnique(received_frames_));
    client_->OnReceiverHasFrames(std::move(received_frames_));
  }
  if (!missing_packets_.empty()) {
    ScrubMissingPacketVector(&missing_packets_);
    if (!missing_packets_.empty()) {
      client_->OnReceiverIsMissingPackets(std::move(missing_packets_));
    }
  }
  if (picture_loss_indicator_) {
    client_->OnReceiverLostPicture();
  }

  return true;
}

bool CompoundRtcpParser::ParseReceiverReport(absl::Span<const uint8_t> in,
                                             int num_report_blocks) {
  if (in.size() < kRtcpReceiverReportSize) {
    return false;
  }
  if (ConsumeField<uint32_t>(&in) == session_->receiver_ssrc()) {
    receiver_report_ = RtcpReportBlock::ParseOne(in, num_report_blocks,
                                                 session_->sender_ssrc());
  }
  return true;
}

bool CompoundRtcpParser::ParseFeedback(absl::Span<const uint8_t> in) {
  // The client must provide, up-front, a reference point for expanding the
  // truncated frame ID values. If missing, it does not intend to process Cast
  // Feedback messages, so just return early.
  if (max_feedback_frame_id_.is_null()) {
    return true;
  }

  if (static_cast<int>(in.size()) < kRtcpFeedbackHeaderSize) {
    return false;
  }
  if (ConsumeField<uint32_t>(&in) != session_->receiver_ssrc() ||
      ConsumeField<uint32_t>(&in) != session_->sender_ssrc()) {
    return true;  // Ignore report from mismatched SSRC(s).
  }
  if (ConsumeField<uint32_t>(&in) != kRtcpCastIdentifierWord) {
    return false;
  }

  const FrameId checkpoint_frame_id =
      max_feedback_frame_id_.ExpandLessThanOrEqual(ConsumeField<uint8_t>(&in));
  const int loss_field_count = ConsumeField<uint8_t>(&in);
  const auto playout_delay =
      std::chrono::milliseconds(ConsumeField<uint16_t>(&in));
  if (!checkpoint_ || checkpoint_->first <= checkpoint_frame_id) {
    checkpoint_.emplace(checkpoint_frame_id, playout_delay);
  }
  if (static_cast<int>(in.size()) <
      (kRtcpFeedbackLossFieldSize * loss_field_count)) {
    return false;
  }

  // Parse the NACKs.
  for (int i = 0; i < loss_field_count; ++i) {
    const FrameId frame_id =
        checkpoint_frame_id.ExpandGreaterThan(ConsumeField<uint8_t>(&in));
    FramePacketId packet_id = ConsumeField<uint16_t>(&in);
    uint8_t bits = ConsumeField<uint8_t>(&in);
    missing_packets_.push_back(PacketNack{frame_id, packet_id});

    if (packet_id != kAllPacketsLost) {
      // Translate each set bit in the bit vector into another missing
      // FramePacketId.
      while (bits) {
        ++packet_id;
        if (bits & 1) {
          missing_packets_.push_back(PacketNack{frame_id, packet_id});
        }
        bits >>= 1;
      }
    }
  }

  // Parse the optional CST2 feedback (frame-level ACKs).
  if (static_cast<int>(in.size()) < kRtcpFeedbackAckHeaderSize ||
      ConsumeField<uint32_t>(&in) != kRtcpCst2IdentifierWord) {
    // Optional CST2 extended feedback is not present. For backwards-
    // compatibility reasons, do not consider any extra "garbage" in the packet
    // that doesn't match 'CST2' as corrupted input.
    return true;
  }
  // Skip over the "Feedback Count" field. It's currently unused, though it
  // might be useful for event tracing later...
  in.remove_prefix(sizeof(uint8_t));
  const int ack_bitvector_octet_count = ConsumeField<uint8_t>(&in);
  if (static_cast<int>(in.size()) < ack_bitvector_octet_count) {
    return false;
  }
  // Translate each set bit in the bit vector into a FrameId.
  FrameId starting_frame_id = checkpoint_frame_id + 2;
  for (int i = 0; i < ack_bitvector_octet_count; ++i) {
    uint8_t bits = ConsumeField<uint8_t>(&in);
    FrameId frame_id = starting_frame_id;
    while (bits) {
      if (bits & 1) {
        received_frames_.push_back(frame_id);
      }
      ++frame_id;
      bits >>= 1;
    }
    constexpr int kBitsPerOctet = 8;
    starting_frame_id += kBitsPerOctet;
  }

  return true;
}

bool CompoundRtcpParser::ParseExtendedReports(absl::Span<const uint8_t> in) {
  if (static_cast<int>(in.size()) < kRtcpExtendedReportHeaderSize) {
    return false;
  }
  if (ConsumeField<uint32_t>(&in) != session_->receiver_ssrc()) {
    return true;  // Ignore report from unknown receiver.
  }

  while (!in.empty()) {
    // All extended report types have the same 4-byte subheader.
    if (static_cast<int>(in.size()) < kRtcpExtendedReportBlockHeaderSize) {
      return false;
    }
    const uint8_t block_type = ConsumeField<uint8_t>(&in);
    in.remove_prefix(sizeof(uint8_t));  // Skip the "reserved" byte.
    const int block_data_size =
        static_cast<int>(ConsumeField<uint16_t>(&in)) * 4;
    if (static_cast<int>(in.size()) < block_data_size) {
      return false;
    }
    if (block_type == kRtcpReceiverReferenceTimeReportBlockType) {
      if (block_data_size != sizeof(uint64_t)) {
        return false;  // Length field must always be 2 words.
      }
      receiver_reference_time_ = session_->ntp_converter().ToLocalTime(
          ReadBigEndian<uint64_t>(in.data()));
    } else {
      // Ignore any other type of extended report.
    }
    in.remove_prefix(block_data_size);
  }

  return true;
}

bool CompoundRtcpParser::ParsePictureLossIndicator(
    absl::Span<const uint8_t> in) {
  if (static_cast<int>(in.size()) < kRtcpPictureLossIndicatorHeaderSize) {
    return false;
  }
  // Only set the flag if the PLI is from the Receiver and to this Sender.
  if (ConsumeField<uint32_t>(&in) == session_->receiver_ssrc() &&
      ConsumeField<uint32_t>(&in) == session_->sender_ssrc()) {
    picture_loss_indicator_ = true;
  }
  return true;
}

CompoundRtcpParser::Client::Client() = default;
CompoundRtcpParser::Client::~Client() = default;
void CompoundRtcpParser::Client::OnReceiverReferenceTimeAdvanced(
    platform::Clock::time_point reference_time) {}
void CompoundRtcpParser::Client::OnReceiverReport(
    const RtcpReportBlock& receiver_report) {}
void CompoundRtcpParser::Client::OnReceiverLostPicture() {}
void CompoundRtcpParser::Client::OnReceiverCheckpoint(
    FrameId frame_id,
    std::chrono::milliseconds playout_delay) {}
void CompoundRtcpParser::Client::OnReceiverHasFrames(
    std::vector<FrameId> acks) {}
void CompoundRtcpParser::Client::OnReceiverIsMissingPackets(
    std::vector<PacketNack> nacks) {}

}  // namespace cast_streaming
}  // namespace openscreen
