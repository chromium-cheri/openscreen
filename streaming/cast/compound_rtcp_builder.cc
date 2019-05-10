// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "streaming/cast/compound_rtcp_builder.h"

#include <algorithm>
#include <iterator>
#include <limits>

#include "osp_base/std_util.h"
#include "platform/api/logging.h"
#include "streaming/cast/constants.h"
#include "streaming/cast/packet_util.h"
#include "streaming/cast/rtcp_session.h"

namespace openscreen {
namespace cast_streaming {

CompoundRtcpBuilder::CompoundRtcpBuilder(RtcpSession* session)
    : session_(session),
      checkpoint_frame_id_(FrameId::first() - 1),
      playout_delay_(kDefaultTargetPlayoutDelay),
      picture_is_lost_(false),
      feedback_count_(0) {
  OSP_DCHECK(session_);
}

CompoundRtcpBuilder::~CompoundRtcpBuilder() = default;

void CompoundRtcpBuilder::SetCheckpointFrame(FrameId frame_id) {
  OSP_DCHECK_GE(frame_id, checkpoint_frame_id_);
  checkpoint_frame_id_ = frame_id;
}

void CompoundRtcpBuilder::SetPlayoutDelay(std::chrono::milliseconds delay) {
  playout_delay_ = delay;
}

void CompoundRtcpBuilder::SetPictureLossIndicator(bool picture_is_lost) {
  picture_is_lost_ = picture_is_lost;
}

void CompoundRtcpBuilder::IncludeReceiverReportInNextPacket(
    const RtcpReportBlock& receiver_report) {
  receiver_report_block_ = receiver_report;
}

void CompoundRtcpBuilder::IncludeFeedbackInNextPacket(
    absl::Span<const FrameId> frame_nacks,
    absl::Span<const std::pair<FrameId, FramePacketId>> packet_nacks,
    absl::Span<const FrameId> frame_acks) {
  // Note: Serialization of these lists will depend on the value of
  // |checkpoint_frame_id_| when BuildPacket() is called later.

  // Merge the NACK data, from |frame_nacks| and |packet_nacks| into |nacks_|,
  // in preparation for more-efficient serialization later.
  OSP_DCHECK(AreElementsSortedAndUnique(frame_nacks));
  OSP_DCHECK(AreElementsSortedAndUnique(packet_nacks));
  nacks_.clear();
  nacks_.reserve(frame_nacks.size() + packet_nacks.size());
  for (size_t i = 0, j = 0;;) {
    if (i == frame_nacks.size()) {
      nacks_.insert(nacks_.end(), packet_nacks.begin() + j, packet_nacks.end());
      break;
    }
    if (j == packet_nacks.size()) {
      std::transform(frame_nacks.begin() + i, frame_nacks.end(),
                     std::back_inserter(nacks_), [](FrameId id) {
                       return std::make_pair(id, kAllPacketsLost);
                     });
      break;
    }
    if (frame_nacks[i] < packet_nacks[j].first) {
      nacks_.emplace_back(frame_nacks[i], kAllPacketsLost);
      ++i;
    } else {
      // Ensure there are no duplicate FrameIds in the two input NACK lists. It
      // wouldn't actually hurt anything other than sending more bytes over the
      // network for no good reason. However, let's not permit sloppiness in
      // upstream code.
      OSP_DCHECK_GT(frame_nacks[i], packet_nacks[j].first);

      nacks_.push_back(packet_nacks[j]);
      ++j;
    }
  }

  OSP_DCHECK(AreElementsSortedAndUnique(frame_acks));
  acks_.assign(frame_acks.begin(), frame_acks.end());

#if OSP_DCHECK_IS_ON()
  // Consistency-check: An ACKed frame should not also be NACKed.
  for (size_t i = 0, j = 0;;) {
    if (i == acks_.size() || j == nacks_.size()) {
      break;
    }
    const FrameId ack_frame_id = acks_[i];
    const FrameId nack_frame_id = nacks_[j].first;
    if (ack_frame_id < nack_frame_id) {
      ++i;
    } else if (nack_frame_id < ack_frame_id) {
      ++j;
    } else {
      OSP_DCHECK_NE(ack_frame_id, nack_frame_id);
    }
  }
#endif
}

absl::Span<uint8_t> CompoundRtcpBuilder::BuildPacket(
    platform::Clock::time_point send_time,
    absl::Span<uint8_t> buffer) {
  OSP_CHECK_GE(buffer.size(), kRequiredBufferSize);

  uint8_t* const packet_begin = buffer.data();

  // Receiver Report: Per RFC 3550, Section 6.4.2, all RTCP compound packets
  // from receivers must include at least an empty receiver report at the start.
  // It's not clear that the Cast RTCP spec requires this, but it costs very
  // little to do so.
  {
    RtcpCommonHeader header;
    header.packet_type = RtcpPacketType::kReceiverReport;
    header.payload_size = kRtcpReceiverReportSize;
    if (receiver_report_block_) {
      header.with.report_count = 1;
      header.payload_size += kRtcpReportBlockSize;
    } else {
      header.with.report_count = 0;
    }
    header.Serialize(buffer);
    buffer.remove_prefix(kRtcpCommonHeaderSize);
    AppendField<uint32_t>(session_->receiver_ssrc(), &buffer);
    if (receiver_report_block_) {
      receiver_report_block_->Serialize(buffer);
      buffer.remove_prefix(kRtcpReportBlockSize);
      receiver_report_block_ = absl::nullopt;
    }
  }

  // Receiver Reference Time Report: While this is optional in the Cast
  // Streaming spec, it is always included by this implementation to improve the
  // stability of the end-to-end system.
  {
    RtcpCommonHeader header;
    header.packet_type = RtcpPacketType::kExtendedReports;
    header.payload_size = kRtcpExtendedReportHeaderSize +
                          kRtcpExtendedReportBlockHeaderSize +
                          kRtcpReceiverReferenceTimeReportBlockSize;
    header.Serialize(buffer);
    buffer.remove_prefix(kRtcpCommonHeaderSize);
    AppendField<uint32_t>(session_->receiver_ssrc(), &buffer);
    AppendField<uint8_t>(kRtcpReceiverReferenceTimeReportBlockType, &buffer);
    AppendField<uint8_t>(0, &buffer);
    AppendField<uint16_t>(
        kRtcpReceiverReferenceTimeReportBlockSize / sizeof(uint32_t), &buffer);
    AppendField<uint64_t>(session_->ntp_converter().ToNtpTimestamp(send_time),
                          &buffer);
  }

  // Picture Loss Indicator: Only included if the flag has been set.
  if (picture_is_lost_) {
    RtcpCommonHeader header;
    header.packet_type = RtcpPacketType::kPayloadSpecific;
    header.with.subtype = RtcpSubtype::kPictureLossIndicator;
    header.payload_size = kRtcpPictureLossIndicatorHeaderSize;
    header.Serialize(buffer);
    buffer.remove_prefix(kRtcpCommonHeaderSize);
    AppendField<uint32_t>(session_->receiver_ssrc(), &buffer);
    AppendField<uint32_t>(session_->sender_ssrc(), &buffer);
  }

  // Cast Feedback: Checkpoint information, and add as many NACKs and ACKs as
  // the remaning space available in the buffer will allow for.
  {
    // Reserve space for the RTCP Common Header. It will be serialized later,
    // after the total size of the Cast Feedback message is known.
    const auto space_for_header = ReserveSpace(kRtcpCommonHeaderSize, &buffer);

    // Append the mandatory fields.
    AppendField<uint32_t>(session_->receiver_ssrc(), &buffer);
    AppendField<uint32_t>(session_->sender_ssrc(), &buffer);
    AppendField<uint32_t>(kRtcpCastIdentifierWord, &buffer);
    AppendField<uint8_t>(checkpoint_frame_id_.lower_8_bits(), &buffer);
    // The |loss_count_field| will be set after the Loss Fields are generated
    // and the total count is known.
    uint8_t* const loss_count_field =
        ReserveSpace(sizeof(uint8_t), &buffer).data();
    OSP_DCHECK_GT(playout_delay_.count(), 0);
    OSP_DCHECK_LE(playout_delay_.count(), std::numeric_limits<uint16_t>::max());
    AppendField<uint16_t>(playout_delay_.count(), &buffer);

    // Try to include as many Loss Fields as possible. Some of the NACKs might
    // be dropped if the remaining space in the buffer is insufficient to
    // include them all.
    int num_loss_fields = 0;
    if (!nacks_.empty()) {
      // The maximum number of entries is limited by available packet buffer
      // space and the 8-bit |loss_count_field|.
      const int max_num_loss_fields =
          std::min<int>(buffer.size() / kRtcpFeedbackLossFieldSize,
                        std::numeric_limits<uint8_t>::max());

      // Translate the |nacks_| list into one or more entries representing
      // specific packet losses.
      OSP_DCHECK(AreElementsSortedAndUnique(nacks_));
      auto it = nacks_.begin();
      for (; it != nacks_.end() && it->first <= checkpoint_frame_id_; ++it)
        ;
      for (; it != nacks_.end() && num_loss_fields != max_num_loss_fields;) {
        const FrameId frame_id = it->first;
        const FramePacketId first_packet_id = it->second;
        uint32_t loss_field = uint32_t{frame_id.lower_8_bits()} << 24;
        loss_field |= uint32_t{first_packet_id} << 8;
        for (++it; it != nacks_.end() && it->first == frame_id; ++it) {
          const int shift = it->second - first_packet_id - 1;
          if (shift >= 8) {
            break;
          }
          loss_field |= 1 << shift;
        }
        AppendField<uint32_t>(loss_field, &buffer);
        ++num_loss_fields;
      }
    }
    OSP_DCHECK_LE(num_loss_fields, std::numeric_limits<uint8_t>::max());
    *loss_count_field = num_loss_fields;

    // Try to include the CST2 header and ACK bit vector. Again, some of the
    // ACKs might be dropped if the remaining space in the buffer is
    // insufficient.
    if (buffer.size() >=
        (kRtcpFeedbackAckHeaderSize + kRtcpMinAckBitVectorOctets)) {
      AppendField<uint32_t>(kRtcpCst2IdentifierWord, &buffer);
      AppendField<uint8_t>(feedback_count_, &buffer);

      // The octet count field is set later, after the total is known.
      uint8_t* const octet_count_field =
          ReserveSpace(sizeof(uint8_t), &buffer).data();

      // Start with the minimum required number of bit vector octets.
      uint8_t* const ack_bitvector =
          ReserveSpace(kRtcpMinAckBitVectorOctets, &buffer).data();
      int num_ack_bitvector_octets = kRtcpMinAckBitVectorOctets;
      memset(ack_bitvector, 0, kRtcpMinAckBitVectorOctets);

      // Set the bits, auto-expanding the number of ACK octets if necessary.
      if (!acks_.empty()) {
        OSP_DCHECK(AreElementsSortedAndUnique(acks_));
        const FrameId first_frame_id = checkpoint_frame_id_ + 2;
        for (const FrameId frame_id : acks_) {
          const int bit_index = frame_id - first_frame_id;
          if (bit_index < 0) {
            continue;
          }
          constexpr int kBitsPerOctet = 8;
          const int octet_index = bit_index / kBitsPerOctet;

          if (octet_index >= num_ack_bitvector_octets) {
            constexpr int kIncrement = sizeof(uint32_t);
            const auto DivideRoundingUp = [](int a, int b) {
              return (a + (b - 1)) / b;
            };
            const int num_additional =
                DivideRoundingUp((octet_index + 1) - num_ack_bitvector_octets,
                                 kIncrement) *
                kIncrement;
            if (static_cast<int>(buffer.size()) < num_additional) {
              break;
            }
            const int new_count = num_ack_bitvector_octets + num_additional;
            if (new_count > kRtcpMaxAckBitVectorOctets) {
              break;
            }
            memset(ReserveSpace(num_additional, &buffer).data(), 0,
                   num_additional);
            num_ack_bitvector_octets = new_count;
          }

          const int shift = bit_index % kBitsPerOctet;
          ack_bitvector[octet_index] |= 1 << shift;
        }
      }

      OSP_DCHECK_LE(num_ack_bitvector_octets,
                    std::numeric_limits<uint8_t>::max());
      *octet_count_field = num_ack_bitvector_octets;
    }

    RtcpCommonHeader header;
    header.packet_type = RtcpPacketType::kPayloadSpecific;
    header.with.subtype = RtcpSubtype::kFeedback;
    uint8_t* const feedback_begin =
        space_for_header.data() + kRtcpCommonHeaderSize;
    uint8_t* const feedback_end = buffer.data();
    header.payload_size = feedback_end - feedback_begin;
    header.Serialize(space_for_header);

    ++feedback_count_;
    nacks_.clear();
    acks_.clear();
  }

  uint8_t* const packet_end = buffer.data();
  return absl::Span<uint8_t>(packet_begin, packet_end - packet_begin);
}

}  // namespace cast_streaming
}  // namespace openscreen
