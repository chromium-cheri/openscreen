// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/clock_offset_estimator_impl.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "platform/base/trivial_clock_traits.h"

namespace openscreen {
namespace cast {
namespace {

template <class M>
typename M::iterator ModMapOldest(M* map) {
  typename M::iterator ret = map->begin();
  if (ret != map->end()) {
    typename M::key_type lower_quarter = 0;
    lower_quarter--;
    lower_quarter >>= 1;
    if (ret->first < lower_quarter) {
      typename M::iterator tmp = map->upper_bound(lower_quarter * 3);
      if (tmp != map->end())
        ret = tmp;
    }
  }
  return ret;
}

// Bitwise merging of values to produce an ordered key for entries in the
// BoundCalculator::events_ map.
uint64_t MakeEventKey(RtpTimeTicks rtp, uint16_t packet_id, bool audio) {
  return (static_cast<uint64_t>(rtp.lower_32_bits()) << 32) |
         (static_cast<uint64_t>(packet_id) << 1) |
         (audio ? UINT64_C(1) : UINT64_C(0));
}

}  // namespace

std::unique_ptr<ClockOffsetEstimator> ClockOffsetEstimator::Create() {
  return std::make_unique<ClockOffsetEstimatorImpl>();
}

ClockOffsetEstimatorImpl::BoundCalculator::BoundCalculator() = default;
ClockOffsetEstimatorImpl::BoundCalculator::BoundCalculator(
    BoundCalculator&&) noexcept = default;
ClockOffsetEstimatorImpl::BoundCalculator&
ClockOffsetEstimatorImpl::BoundCalculator::operator=(BoundCalculator&&) =
    default;
ClockOffsetEstimatorImpl::BoundCalculator::~BoundCalculator() = default;

void ClockOffsetEstimatorImpl::BoundCalculator::SetSent(RtpTimeTicks rtp,
                                                        uint16_t packet_id,
                                                        bool audio,
                                                        Clock::time_point t) {
  const uint64_t key = MakeEventKey(rtp, packet_id, audio);
  events_[key].first = t;
  CheckUpdate(key);
}

void ClockOffsetEstimatorImpl::BoundCalculator::SetReceived(
    RtpTimeTicks rtp,
    uint16_t packet_id,
    bool audio,
    Clock::time_point t) {
  const uint64_t key = MakeEventKey(rtp, packet_id, audio);
  events_[key].second = t;
  CheckUpdate(key);
}

void ClockOffsetEstimatorImpl::BoundCalculator::UpdateBound(
    Clock::time_point sent,
    Clock::time_point received) {
  Clock::duration delta = received - sent;
  if (has_bound_) {
    if (delta < bound_) {
      bound_ = delta;
    } else {
      bound_ += (delta - bound_) / kClockDriftSpeed;
    }
  } else {
    bound_ = delta;
  }
  has_bound_ = true;
}

void ClockOffsetEstimatorImpl::BoundCalculator::CheckUpdate(uint64_t key) {
  const TimeTickPair& ticks = events_[key];
  if (ticks.first && ticks.second) {
    UpdateBound(ticks.first.value(), ticks.second.value());
    events_.erase(key);
    return;
  }

  if (events_.size() > kMaxEventTimesMapSize) {
    auto i = ModMapOldest(&events_);
    if (i != events_.end()) {
      events_.erase(i);
    }
  }
}

ClockOffsetEstimatorImpl::ClockOffsetEstimatorImpl() = default;
ClockOffsetEstimatorImpl::ClockOffsetEstimatorImpl(
    ClockOffsetEstimatorImpl&&) noexcept = default;
ClockOffsetEstimatorImpl& ClockOffsetEstimatorImpl::operator=(
    ClockOffsetEstimatorImpl&&) = default;
ClockOffsetEstimatorImpl::~ClockOffsetEstimatorImpl() = default;

void ClockOffsetEstimatorImpl::OnFrameEvent(const FrameEvent& frame_event) {
  switch (frame_event.type) {
    case StatisticsEventType::kFrameAckSent:
      lower_bound_.SetSent(
          frame_event.rtp_timestamp, 0,
          frame_event.media_type == StatisticsEventMediaType::kAudio,
          frame_event.timestamp);
      break;
    case StatisticsEventType::kFrameAckReceived:
      lower_bound_.SetReceived(
          frame_event.rtp_timestamp, 0,
          frame_event.media_type == StatisticsEventMediaType::kAudio,
          frame_event.timestamp);
      break;
    default:
      // Ignored
      break;
  }
}

bool ClockOffsetEstimatorImpl::GetReceiverOffsetBounds(
    Clock::duration& lower_bound,
    Clock::duration& upper_bound) const {
  if (!lower_bound_.has_bound() || !upper_bound_.has_bound()) {
    return false;
  }

  lower_bound = -lower_bound_.bound();
  upper_bound = upper_bound_.bound();

  // Sanitize the output, we don't want the upper bound to be
  // lower than the lower bound, make them the same.
  if (upper_bound < lower_bound) {
    lower_bound += (lower_bound - upper_bound) / 2;
    upper_bound = lower_bound;
  }
  return true;
}

absl::optional<Clock::duration> ClockOffsetEstimatorImpl::GetEstimatedOffset()
    const {
  Clock::duration lower_bound;
  Clock::duration upper_bound;
  if (!GetReceiverOffsetBounds(lower_bound, upper_bound)) {
    return {};
  }
  return (upper_bound + lower_bound) / 2;
}

void ClockOffsetEstimatorImpl::OnPacketEvent(const PacketEvent& packet_event) {
  switch (packet_event.type) {
    case StatisticsEventType::kPacketSentToNetwork:
      upper_bound_.SetSent(
          packet_event.rtp_timestamp, packet_event.packet_id,
          packet_event.media_type == StatisticsEventMediaType::kAudio,
          packet_event.timestamp);
      break;
    case StatisticsEventType::kPacketReceived:
      upper_bound_.SetReceived(
          packet_event.rtp_timestamp, packet_event.packet_id,
          packet_event.media_type == StatisticsEventMediaType::kAudio,
          packet_event.timestamp);
      break;
    default:
      // Ignored
      break;
  }
}

}  // namespace cast
}  // namespace openscreen
