// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_CLOCK_OFFSET_ESTIMATOR_H_
#define CAST_STREAMING_CLOCK_OFFSET_ESTIMATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <utility>

#include "absl/types/optional.h"
#include "cast/streaming/rtp_time.h"
#include "cast/streaming/statistics_defines.h"
#include "platform/base/trivial_clock_traits.h"

namespace openscreen {
namespace cast {

// This should be large enough so that we can collect all 3 events before
// the entry gets removed from the map.
constexpr size_t kMaxEventTimesMapSize = 500;

// The lower, this is, the faster we adjust to clock drift.
// (But with more jitter.)
constexpr size_t kClockDriftSpeed = 500;

// This implementation listens to two pair of events
// 1. FrameAckSent / FrameAckReceived (receiver->sender)
// 2. PacketSentToNetwork / PacketReceived (sender->receiver)
// There is a causal relationship between these events in that these events
// must happen in order. This class obtains the lower and upper bounds for
// the offset by taking the difference of timestamps.
class ClockOffsetEstimator {
 public:
  ClockOffsetEstimator();
  ClockOffsetEstimator(ClockOffsetEstimator&&) noexcept;
  ClockOffsetEstimator(const ClockOffsetEstimator&) = delete;
  ClockOffsetEstimator& operator=(ClockOffsetEstimator&&);
  ClockOffsetEstimator& operator=(const ClockOffsetEstimator&) = delete;
  ~ClockOffsetEstimator();

  void OnFrameEvent(const FrameEvent& frame_event);
  void OnPacketEvent(const PacketEvent& packet_event);

  // TODO(jophba): remove from API after tests are updated to use
  // GetEstimatedOffset() instead.
  bool GetReceiverOffsetBounds(Clock::duration& lower_bound,
                               Clock::duration& upper_bound) const;

  // Returns nullopt if not enough data is in yet to produce an estimate.
  absl::optional<Clock::duration> GetEstimatedOffset() const;

 private:
  // This helper uses the difference between sent and received event
  // to calculate an upper bound on the difference between the clocks
  // on the sender and receiver. Note that this difference can take
  // very large positive or negative values, but the smaller value is
  // always the better estimate, since a receive event cannot possibly
  // happen before a send event.  Note that we use this to calculate
  // both upper and lower bounds by reversing the sender/receiver
  // relationship.
  class BoundCalculator {
   public:
    typedef std::pair<absl::optional<Clock::time_point>,
                      absl::optional<Clock::time_point>>
        TimeTickPair;
    typedef std::map<uint64_t, TimeTickPair> EventMap;

    BoundCalculator();
    ~BoundCalculator();
    bool has_bound() const { return has_bound_; }
    Clock::duration bound() const { return bound_; }

    void SetSent(RtpTimeTicks rtp,
                 uint16_t packet_id,
                 bool audio,
                 Clock::time_point t);

    void SetReceived(RtpTimeTicks rtp,
                     uint16_t packet_id,
                     bool audio,
                     Clock::time_point t);

   private:
    void UpdateBound(Clock::time_point a, Clock::time_point b);
    void CheckUpdate(uint64_t key);

   private:
    EventMap events_;
    bool has_bound_ = false;
    Clock::duration bound_;
  };

  // Fixed size storage to store event times for recent frames.
  BoundCalculator upper_bound_;
  BoundCalculator lower_bound_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_CLOCK_OFFSET_ESTIMATOR_H_
