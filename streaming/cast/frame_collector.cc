// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "streaming/cast/frame_collector.h"

#include <algorithm>
#include <limits>

#include "platform/api/logging.h"
#include "streaming/cast/frame_id.h"
#include "streaming/cast/rtp_defines.h"

namespace openscreen {
namespace cast_streaming {

namespace {

// Integer constant representing that the number of packets is not yet known.
constexpr int kUnknownNumberOfPackets = std::numeric_limits<int>::max();

// Integer constant representing a missing chunk of frame data.
constexpr int kMissingOffset = -1;

}  // namespace

FrameCollector::FrameCollector()
    : num_missing_packets_(kUnknownNumberOfPackets) {}

FrameCollector::~FrameCollector() = default;

bool FrameCollector::CollectPart(const RtpPacketParser::ParseResult& part) {
  OSP_DCHECK(!frame_.frame_id.is_null());

  const int frame_packet_count = static_cast<int>(part.max_packet_id) + 1;
  if (num_missing_packets_ == kUnknownNumberOfPackets) {
    // This is the first packet being processed for the frame.
    if (part.frame_id != frame_.frame_id) {
      OSP_LOG_WARN
          << "Ignoring [corrupt?] packet with frame ID mismatch. Expected: "
          << frame_.frame_id << " Got: " << part.frame_id;
      return false;
    }
    num_missing_packets_ = frame_packet_count;
    chunks_.assign(num_missing_packets_, DataChunkSpan{kMissingOffset, 0});
  } else {
    // Since this is not the first packet being processed, sanity-check that the
    // "frame ID" and "max packet ID" are the expected values.
    if (part.frame_id != frame_.frame_id ||
        frame_packet_count != static_cast<int>(chunks_.size())) {
      OSP_LOG_WARN
          << "Ignoring [corrupt?] packet with frame ID or packet count "
             "mismatch. Expected: "
          << frame_.frame_id << ':' << chunks_.size()
          << " Got: " << part.frame_id << ':' << frame_packet_count;
      return false;
    }
  }

  // The packet ID must not be greater than the max packet ID.
  if (part.packet_id >= chunks_.size()) {
    return false;
  }

  // Don't process duplicate packets.
  if (chunks_[part.packet_id].offset != kMissingOffset) {
    return true;
  }

  // Populate metadata from packet 0 only, which is the only packet that must
  // contain a complete set of values.
  if (part.packet_id == FramePacketId{0}) {
    if (part.is_key_frame) {
      frame_.dependency = EncodedFrame::Dependency::KEY;
    } else if (part.frame_id == part.referenced_frame_id) {
      frame_.dependency = EncodedFrame::Dependency::INDEPENDENT;
    } else {
      frame_.dependency = EncodedFrame::Dependency::DEPENDENT;
    }
    frame_.referenced_frame_id = part.referenced_frame_id;
    frame_.rtp_timestamp = part.rtp_timestamp;
    frame_.new_playout_delay = part.new_playout_delay;
  }

  // Copy the packet's payload into frame storage. For efficiency, just append
  // the data to the end. Later, PeekAtAssembledFrame() will shuffle it all back
  // into the correct order if necessary.
  chunks_[part.packet_id] =
      DataChunkSpan{static_cast<int>(frame_.owned_data_.size()),
                    static_cast<int>(part.payload.size())};
  frame_.owned_data_.insert(frame_.owned_data_.end(), part.payload.begin(),
                            part.payload.end());
  --num_missing_packets_;
  OSP_DCHECK_GE(num_missing_packets_, 0);
  return true;
}

void FrameCollector::AppendMissingPackets(
    std::vector<PacketNack>* nacks) const {
  OSP_DCHECK(!frame_.frame_id.is_null());

  if (num_missing_packets_ == 0) {
    return;
  }

  if (num_missing_packets_ >= static_cast<int>(chunks_.size())) {
    nacks->push_back(PacketNack{frame_.frame_id, kAllPacketsLost});
    return;
  }

  for (int packet_id = 0; packet_id < static_cast<int>(chunks_.size());
       ++packet_id) {
    if (chunks_[packet_id].offset == kMissingOffset) {
      nacks->push_back(
          PacketNack{frame_.frame_id, static_cast<FramePacketId>(packet_id)});
    }
  }
}

const EncryptedFrame& FrameCollector::PeekAtAssembledFrame() {
  OSP_DCHECK_EQ(num_missing_packets_, 0);

  if (!frame_.data.data()) {
    // If the parts of the frame were collected out-of-order, re-order them now.
    if (!std::is_sorted(chunks_.begin(), chunks_.end())) {
      std::vector<uint8_t> data_in_order;
      data_in_order.reserve(frame_.owned_data_.size());
      for (int i = 0; i < static_cast<int>(chunks_.size()); ++i) {
        const auto begin = frame_.owned_data_.begin() + chunks_[i].offset;
        const auto end = begin + chunks_[i].size;
        data_in_order.insert(data_in_order.end(), begin, end);
      }
      frame_.owned_data_.swap(data_in_order);
    }
    frame_.data = absl::Span<uint8_t>(frame_.owned_data_);
  }

  return frame_;
}

void FrameCollector::Reset() {
  num_missing_packets_ = kUnknownNumberOfPackets;
  frame_.frame_id = FrameId();
  frame_.owned_data_.clear();
  frame_.owned_data_.shrink_to_fit();
  frame_.data = absl::Span<uint8_t>();
}

}  // namespace cast_streaming
}  // namespace openscreen
