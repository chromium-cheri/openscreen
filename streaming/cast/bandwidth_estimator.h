// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_BANDWIDTH_ESTIMATOR_H_
#define STREAMING_CAST_BANDWIDTH_ESTIMATOR_H_

#include <stdint.h>

#include <limits>
#include <utility>

#include "absl/types/optional.h"
#include "platform/api/time.h"
#include "util/yet_another_bit_vector.h"

namespace openscreen {
namespace cast_streaming {

// Tracks metrics related to the recent planned, executed, and successful
// transmission activity; and then provides logic to compute the effective
// current bitrate (i.e., successful transmission) and remaining available
// bitrate over a time range.
//
// The logic works without an awareness of how the data is being sent, the
// sources of overhead involved in transmission and acknowledgement, or
// when/whether any data must be re-transmitted. Instead, the focus is primarily
// on the rough time windows over which the payload bytes have/will move from
// source to destination. Recent success rates and the duration of transmission
// attempts are tracked to determine the current average effective bitrate.
// Also, encumberances on bandwidth are tracked to be able to determine the
// remaining available bitrate for additional payloads.
class BandwidthEstimator {
 public:
  BandwidthEstimator();
  ~BandwidthEstimator();

  // Record the [begin,end) window of time over which some number of
  // |payload_bytes| will be sent. This should only be called once for the same
  // payload, and not be called to track re-transmits or otherwise account for
  // any other protocol overhead.
  void RecordFlightPlan(int payload_bytes,
                        platform::Clock::time_point begin,
                        platform::Clock::time_point end);

  // Record |when| burst-sending was active/inactive.
  void RecordActiveBurstTime(platform::Clock::time_point when);
  void RecordInactiveBurstTime(platform::Clock::time_point when);

  // Record that some number of payload bytes has been acknowledged as
  // successfully received. The second argument is the time at which the last
  // packet contributing to the success had departed from the Sender (i.e., the
  // time the Sender received ACK feedback minus one network round-trip).
  //
  // It's also important Senders call this any time any feedback comes in from
  // the Receiver, even if zero bytes were acknowledged, since this window of
  // "nothing received" is also important information to track.
  void RecordFeedback(int payload_bytes_acknowledged,
                      platform::Clock::time_point feedback_time_minus_rtt);

  // Computes the effective bitrate--the rate at which bits are being
  // successfully received at the Receiver--based on recent tracking data.
  // Returns nullopt if this cannot be determined due to a lack of
  // sufficiently-recent data.
  absl::optional<int> ComputeEffectiveBitrate() const;

  // Predicts the maximum bitrate available for additional payload data over the
  // given [begin,end) window of time. Returns nullopt if this cannot be
  // determined due to a lack of sufficiently-recent data.
  absl::optional<int> PredictAvailableBitrate(
      platform::Clock::time_point begin,
      platform::Clock::time_point end) const;

 private:
  // Parameters defining the granularity and range of recent history tracked by
  // ActivityTracker and FlowTracker.
  static constexpr int kNumBuckets = 256;
  static constexpr platform::Clock::duration kBucketPeriod =
      std::chrono::milliseconds{10};
  static constexpr platform::Clock::duration kHistoryDuration =
      kBucketPeriod * kNumBuckets;

  // Tracks recent activity over a fixed range of time. The range of time is
  // divided up into a fixed number of buckets, each of which represents whether
  // there was any activity (a YES/NO metric) during a certain period of time.
  class ActivityTracker {
   public:
    ActivityTracker();
    ~ActivityTracker();

    platform::Clock::time_point begin_time() const { return begin_time_; }
    platform::Clock::time_point end_time() const {
      return begin_time_ + kHistoryDuration;
    }

    // Advance the end of the time window being tracked such that the
    // most-recent bucket's time period includes |until|. Old buckets are
    // dropped and new ones are initialized as "inactive" periods of time.
    void AdvanceToIncludeTime(platform::Clock::time_point until);

    // Mark the bucket that includes |when| in its time range as an active
    // period of time.
    void MarkActive(platform::Clock::time_point when);

    // Return the total amount of time that is marked as active in recent
    // history. Divide this by |duration()| to get the fraction of time that was
    // active.
    platform::Clock::duration SumActiveTime() const;

   private:
    // Bit 0 represents the bucket starting at |begin_time_|, and the highest
    // bit represents the most recent bucket (the one ending at |end_time()|).
    YetAnotherBitVector buckets_;
    platform::Clock::time_point begin_time_{platform::Clock::time_point::min()};
  };

  // Tracks recent volume (i.e., any integer metric) over a fixed range of time.
  // The range of time is divided up into a fixed number of buckets, each of
  // which represents the total number of bytes that flowed during a certain
  // periods of time.
  class FlowTracker {
   public:
    FlowTracker();
    ~FlowTracker();

    platform::Clock::time_point begin_time() const { return begin_time_; }
    platform::Clock::time_point end_time() const {
      return begin_time_ + kHistoryDuration;
    }

    // Advance the end of the time window being tracked such that the
    // most-recent bucket's time period includes |until|. Old buckets are
    // dropped and new ones are initialized to a zero amount.
    void AdvanceToIncludeTime(platform::Clock::time_point until);

    // Accumulate the given |amount| into the bucket whose time period includes
    // |when|.
    void Accumulate(int32_t amount, platform::Clock::time_point when);

    // Accumulate the given |amount| by spreading it evenly over the buckets
    // covered enough by the given [begin,end) window of time. See comments for
    // ToRelativeIndexRange() for further explanation of what is meant by
    // "covered enough."
    void AccumulateOverRange(int32_t amount,
                             platform::Clock::time_point begin,
                             platform::Clock::time_point end);

    // Return the sum of all the amounts in recent history. Divide this by
    // |kHistoryDuration| to get the average flow rate for recent history.
    int32_t Sum() const;

    // Return the sum of of all the amounts in the buckets covered enough by the
    // given [begin,end) window of time. See comments for ToRelativeIndexRange()
    // for further explanation of what is meant by "covered enough."
    int32_t SumOverRange(platform::Clock::time_point begin,
                         platform::Clock::time_point end) const;

   private:
    // FlowTracker manages a ring buffer of size 256. It simplifies the index
    // calculations to use an integer data type where all arithmetic is mod 256.
    using uint_mod_256 = uint8_t;
    static_assert(std::numeric_limits<uint_mod_256>::max() == (kNumBuckets - 1),
                  "Some FlowTracker code assumes modular arithmetic.");

    // Computes the indices (relative to the first bucket) of the buckets to sum
    // over. The indices are rounded up or down such that a bucket is only
    // included in the range if >= 50% of its time range overlaps with
    // [begin,end). Note that the values returned here may exceed the valid
    // range of [0,kNumBuckets).
    std::pair<int64_t, int64_t> ToRelativeIndexRange(
        platform::Clock::time_point begin,
        platform::Clock::time_point end) const;

    // Ring buffer tracking the accumulated amounts.
    int32_t ring_of_buckets_[kNumBuckets]{};

    // The index of the oldest bucket in |ring_of_buckets_|. This can also be
    // thought of, equivalently, as the index just after the newest bucket mod
    // |kNumBuckets|.
    uint_mod_256 tail_ = 0;

    // The begin time of the oldest bucket.
    platform::Clock::time_point begin_time_{platform::Clock::time_point::min()};
  };

  // History tracking for recent planned, executed, and successful transmission
  // activity.
  FlowTracker flight_plan_;
  ActivityTracker burst_history_;
  FlowTracker feedback_history_;
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_BANDWIDTH_ESTIMATOR_H_
