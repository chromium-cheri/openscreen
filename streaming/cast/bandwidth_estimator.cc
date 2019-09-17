// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "streaming/cast/bandwidth_estimator.h"

#include <algorithm>

#include "platform/api/logging.h"

namespace openscreen {
namespace cast_streaming {

namespace {

constexpr platform::Clock::duration kDurationOfOneClockTick{1};

// Convert from one value type to another, clamping to the min/max of the new
// value type's range if necessary.
template <typename To, typename From>
constexpr To saturate_cast(From from) {
  if (from >= std::numeric_limits<To>::max()) {
    return std::numeric_limits<To>::max();
  }
  if (from <= std::numeric_limits<To>::min()) {
    return std::numeric_limits<To>::min();
  }
  return static_cast<To>(from);
}

// Converts units from |bytes| per |time_window| number of Clock ticks into
// bits-per-second.
int ToClampedBitsPerSecond(int32_t bytes,
                           platform::Clock::duration time_window) {
  OSP_DCHECK(time_window > platform::Clock::duration::zero());

  // Divide |bytes| by |time_window| and scale the units to bits per second.
  constexpr int64_t kBitsPerByte = 8;
  constexpr int64_t kClockTicksPerSecond =
      std::chrono::duration_cast<platform::Clock::duration>(
          std::chrono::seconds(1))
          .count();
  const int64_t bits = bytes * kBitsPerByte;
  const int64_t bits_per_second =
      (bits * kClockTicksPerSecond) / time_window.count();
  return saturate_cast<int>(bits_per_second);
}

}  // namespace

BandwidthEstimator::BandwidthEstimator() = default;
BandwidthEstimator::~BandwidthEstimator() = default;

void BandwidthEstimator::RecordFlightPlan(int payload_bytes,
                                          platform::Clock::time_point begin,
                                          platform::Clock::time_point end) {
  if (begin < end) {
    // Normal case: Transmission will be paced over a range of time.
    flight_plan_.AccumulateOverRange(payload_bytes, begin, end);
  } else {
    // Late case: Track this event as "sending ASAP, all at once."
    flight_plan_.Accumulate(payload_bytes, begin);
  }
}

void BandwidthEstimator::RecordActiveBurstTime(
    platform::Clock::time_point when) {
  burst_history_.MarkActive(when);
}

void BandwidthEstimator::RecordInactiveBurstTime(
    platform::Clock::time_point when) {
  burst_history_.AdvanceToIncludeTime(when);
}

void BandwidthEstimator::RecordFeedback(
    int payload_bytes_acknowledged,
    platform::Clock::time_point feedback_time_minus_rtt) {
  OSP_DCHECK_GE(payload_bytes_acknowledged, 0);
  feedback_history_.Accumulate(payload_bytes_acknowledged,
                               feedback_time_minus_rtt);
}

absl::optional<int> BandwidthEstimator::ComputeEffectiveBitrate() const {
  // Determine whether the |burst_history_| time range overlaps in time with the
  // |feedback_history_| time range "enough." The time ranges don't have to
  // overlap perfectly because the desired result is just a "recent average."
  const platform::Clock::time_point overlap_begin =
      std::max(burst_history_.begin_time(), feedback_history_.begin_time());
  const platform::Clock::time_point overlap_end =
      std::min(burst_history_.end_time(), feedback_history_.end_time());
  if ((overlap_end - overlap_begin) < (kHistoryDuration / 2)) {
    // Not enough recent overlap between |burst_history_| and
    // |feedback_history_|.
    return absl::nullopt;
  }

  // The transmit duration is the entire history duration minus the "dead time."
  const platform::Clock::duration transmit_duration =
      burst_history_.SumActiveTime();
  if (transmit_duration <= platform::Clock::duration::zero()) {
    // The bitrate cannot be determined because there have been no transmissions
    // recently.
    return absl::nullopt;
  }

  return ToClampedBitsPerSecond(feedback_history_.Sum(), transmit_duration);
}

absl::optional<int> BandwidthEstimator::PredictAvailableBitrate(
    platform::Clock::time_point begin,
    platform::Clock::time_point end) const {
  OSP_DCHECK(begin < end);

  // Start with the rate at which bytes have proven to make it from end to end.
  // Then, subtract the encumbrance, the part of the total available bandwidth
  // which has already been committed.
  const absl::optional<int> effective_bitrate = ComputeEffectiveBitrate();
  if (!effective_bitrate) {
    return absl::nullopt;
  }
  return *effective_bitrate -
         ToClampedBitsPerSecond(flight_plan_.SumOverRange(begin, end),
                                end - begin);
}

BandwidthEstimator::ActivityTracker::ActivityTracker()
    : buckets_(kNumBuckets, YetAnotherBitVector::CLEARED) {}
BandwidthEstimator::ActivityTracker::~ActivityTracker() = default;

void BandwidthEstimator::ActivityTracker::AdvanceToIncludeTime(
    platform::Clock::time_point until) {
  if (until < end_time()) {
    return;  // Not advancing.
  }

  // Discard N oldest buckets, and create N new ones, such that the newest
  // bucket will hold the state for |until|.
  const int64_t num_new_buckets = 1 + (until - end_time()) / kBucketPeriod;
  if (num_new_buckets < kNumBuckets) {
    buckets_.ShiftRight(num_new_buckets);
  } else {
    buckets_.ClearAll();
  }
  begin_time_ += num_new_buckets * kBucketPeriod;
}

void BandwidthEstimator::ActivityTracker::MarkActive(
    platform::Clock::time_point when) {
  OSP_DCHECK(when > platform::Clock::time_point::min());
  AdvanceToIncludeTime(when);
  if (when >= begin_time_) {
    const int which = static_cast<int>((when - begin_time_) / kBucketPeriod);
    buckets_.Set(which);
  }
}

platform::Clock::duration BandwidthEstimator::ActivityTracker::SumActiveTime()
    const {
  return kBucketPeriod * buckets_.CountBitsSet(0, kNumBuckets);
}

BandwidthEstimator::FlowTracker::FlowTracker() = default;
BandwidthEstimator::FlowTracker::~FlowTracker() = default;

void BandwidthEstimator::FlowTracker::AdvanceToIncludeTime(
    platform::Clock::time_point until) {
  if (until < end_time()) {
    return;  // Not advancing.
  }

  // Discard N oldest buckets, and create N new ones, such that the newest
  // bucket will hold the state for |until|.
  int64_t num_new_buckets = 1 + (until - end_time()) / kBucketPeriod;
  begin_time_ += num_new_buckets * kBucketPeriod;
  if (num_new_buckets < kNumBuckets) {
    for (; num_new_buckets > 0; --num_new_buckets) {
      ring_of_buckets_[tail_] = 0;
      ++tail_;
    }
  } else {
    // Just start over, since all existing buckets are being dropped and new
    // ones are taking their place. Note that |tail_| is not adjusted, since it
    // doesn't matter which bucket in the ring is the first bucket.
    std::fill(&ring_of_buckets_[0], &ring_of_buckets_[kNumBuckets], 0);
  }
}

void BandwidthEstimator::FlowTracker::Accumulate(
    int32_t amount,
    platform::Clock::time_point when) {
  OSP_DCHECK(when > platform::Clock::time_point::min());
  AdvanceToIncludeTime(when);
  if (when < begin_time_) {
    return;  // Ignore a data point that is already too old.
  }
  const int64_t offset_from_first = (when - begin_time_) / kBucketPeriod;
  const uint_mod_256 ring_index = tail_ + offset_from_first;
  int32_t& bucket = ring_of_buckets_[ring_index];
  bucket = saturate_cast<int32_t>(int64_t{bucket} + amount);
}

void BandwidthEstimator::FlowTracker::AccumulateOverRange(
    int32_t amount,
    platform::Clock::time_point begin,
    platform::Clock::time_point end) {
  OSP_DCHECK(begin > platform::Clock::time_point::min());
  OSP_DCHECK(begin < end);

  AdvanceToIncludeTime(end - kDurationOfOneClockTick);
  if (end <= begin_time_) {
    return;  // Ignore |amount| because the time range is already too old.
  }

  // Compute the number of buckets over which to spread the |amount| and the
  // portion to add to each.
  const auto relative_range = ToRelativeIndexRange(begin, end);
  const int64_t spread =
      std::max<int64_t>(relative_range.second - relative_range.first, 1);
  const int32_t amount_per_bucket = amount / spread;
  const int32_t leftover_amount = amount % spread;

  // Add the per-bucket amount to each bucket whose representative time range
  // intersects with [begin,end). The last bucket also gets the
  // |leftover_amount|.
  const int64_t relative_offset_begin =
      std::max<int64_t>(relative_range.first, 0);
  const int64_t relative_offset_end =
      std::min<int64_t>(relative_range.second, kNumBuckets);
  const uint_mod_256 ring_index_last = tail_ + relative_offset_end - 1;
  for (uint_mod_256 ring_index = tail_ + relative_offset_begin;
       ring_index != ring_index_last; ++ring_index) {
    ring_of_buckets_[ring_index] = saturate_cast<int32_t>(
        int64_t{ring_of_buckets_[ring_index]} + amount_per_bucket);
  }
  ring_of_buckets_[ring_index_last] =
      saturate_cast<int32_t>(int64_t{ring_of_buckets_[ring_index_last]} +
                             amount_per_bucket + leftover_amount);
}

int32_t BandwidthEstimator::FlowTracker::Sum() const {
  int64_t result = 0;
  for (int32_t bucket_value : ring_of_buckets_) {
    result += bucket_value;
  }
  return saturate_cast<int32_t>(result);
}

int32_t BandwidthEstimator::FlowTracker::SumOverRange(
    platform::Clock::time_point begin,
    platform::Clock::time_point end) const {
  // Get the relative bucket index range representative of the time range, and
  // clamp to the valid bucket index range.
  const auto relative_range = ToRelativeIndexRange(begin, end);
  if (relative_range.first >= relative_range.second) {
    return 0;  // Empty range sums to zero.
  }
  const int64_t relative_offset_begin =
      std::max<int64_t>(relative_range.first, 0);
  const int64_t relative_offset_end =
      std::min<int64_t>(relative_range.second, kNumBuckets);

  int64_t result = 0;
  const uint_mod_256 ring_index_end = tail_ + relative_offset_end;
  for (uint_mod_256 ring_index = tail_ + relative_offset_begin;
       ring_index != ring_index_end; ++ring_index) {
    result += ring_of_buckets_[ring_index];
  }
  return saturate_cast<int32_t>(result);
}

std::pair<int64_t, int64_t>
BandwidthEstimator::FlowTracker::ToRelativeIndexRange(
    platform::Clock::time_point begin,
    platform::Clock::time_point end) const {
  return std::make_pair(
      (begin - begin_time_ + kBucketPeriod / 2) / kBucketPeriod,
      (end - begin_time_ + kBucketPeriod / 2) / kBucketPeriod);
}

}  // namespace cast_streaming
}  // namespace openscreen
