// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_BANDWIDTH_ESTIMATOR_H_
#define CAST_STREAMING_BANDWIDTH_ESTIMATOR_H_

#include <stdint.h>

#include <limits>

#include "platform/api/time.h"

namespace openscreen {
namespace cast {

// Tracks send attempts and successful receives, and then computes a total
// network bandwith estimate.
//
// Rather than track interesting events using fine-grained moments in time, the
// BandwidthEstimator tracks recent history using a ring of timeslice buckets.
// These timeslice buckets should have the same duration as the burst interval
// configured in the SenderPacketRouter.
//
// Two things are tracked by the BandwidthEstimator:
//
//   1. The number of packets sent during bursts (see SenderPacketRouter for
//      explanation of what a "burst" is). These track when the network was
//      actually in-use for transmission and the magnitude of each burst. When
//      computing bandwidth, the estimator assumes the timeslices where the
//      network was not in-use could have been used to send even more bytes at
//      the same rate.
//
//   2. Transmission completion over time. Packets that are acknowledged by the
//      Receiver are providing proof of the successful receipt of payload bytes
//      over time.
//
// The BandwidthEstimator assumes a simplified model for network transmission.
// It focuses entirely on the transmission of the media payload over time, and
// not any of the protocol overhead. It is also not aware of packet
// re-transmits, but it does implicitly account for network reliability, as the
// numbers can't lie.
//
// The simplified model produces some known inaccuracies in the resulting
// estimations. First, the estimator is only reasonably accurate while a
// sufficient amount data is being transmitted. If no data has recently
// transmitted, estimations cannot be provided. If the transmission rate is much
// smaller than the true network capacity, the logic will tend to
// under-estimate. However, those estimates will still be far larger than the
// transmission rate. Finally, if the transmission rate is near (or exceeding)
// the limits of the network, the estimations will be very accurate.
//
// Despite the inaccuracies in the estimates, they can be used effectively as a
// control signal in upstream code modules. The media encoding target bitrate
// should be adjusted in realtime using a TCP-like congestion control algorithm:
//
//   1. When the estimated bitrate is less than the current encoding target
//      bitrate, aggressively and immediately decrease the encoding bitrate.
//
//   2. When the estimated bitrate is more than the current encoding target
//      bitrate, gradually increase the encoding bitrate.
class BandwidthEstimator {
 public:
  // |max_packets_per_timeslice| and |timeslice_duration| should match the
  // configuration in SenderPacketRouter. |start_time| should be a recent
  // point-in-time before the first packet is sent.
  BandwidthEstimator(int max_packets_per_timeslice,
                     Clock::duration timeslice_duration,
                     Clock::time_point start_time);

  ~BandwidthEstimator();

  Clock::duration history_window() const { return history_window_; }

  // Records |when| burst-sending was active or inactive. For the active case,
  // |num_packets| should include all network packets sent, including
  // non-payload packets (since both affect the modeled utilization/capacity).
  // For the inactive case, this method should be called with zero for
  // |num_packets|.
  void OnBurstComplete(int num_packets_sent, Clock::time_point when);

  // Records when a RTCP packet was received. It's important for Senders to call
  // this any time a packet comes in from the Receiver, even if no payload is
  // being acknowledged, since the time windows of "nothing successfully
  // received" is also important information to track.
  void OnRtcpReceived(Clock::time_point arrival_time,
                      Clock::duration estimated_round_trip_time);

  // Records that some number of payload bytes has been acknowledged (i.e.,
  // successfully received).
  void OnPayloadReceived(int payload_bytes_acknowledged,
                         Clock::time_point ack_arrival_time,
                         Clock::duration estimated_round_trip_time);

  // Computes the current network bandwith estimate. Returns 0 if this cannot be
  // determined due to a lack of sufficiently-recent data.
  int ComputeNetworkBandwidth() const;

 private:
  // FlowTracker (below) manages a ring buffer of size 256. It simplifies the
  // index calculations to use an integer data type where all arithmetic is mod
  // 256.
  using index_mod_256_t = uint8_t;
  static constexpr int kNumBuckets =
      static_cast<int>(std::numeric_limits<index_mod_256_t>::max()) + 1;

  // Tracks volume (e.g., the total number of payload bytes) over a fixed
  // recent-history time window. The time window is divided up into a fixed
  // number of buckets, each of which represents the total number of bytes that
  // flowed during a certain period of time.
  class FlowTracker {
   public:
    FlowTracker(Clock::duration bucket_duration, Clock::time_point begin_time);
    ~FlowTracker();

    Clock::time_point begin_time() const { return begin_time_; }
    Clock::time_point end_time() const {
      return begin_time_ + bucket_duration_ * kNumBuckets;
    }

    // Advance the end of the time window being tracked such that the
    // most-recent bucket's time period includes |until|. Old buckets are
    // dropped and new ones are initialized to a zero amount.
    void AdvanceToIncludeTime(Clock::time_point until);

    // Accumulate the given |amount| into the bucket whose time period includes
    // |when|.
    void Accumulate(int32_t amount, Clock::time_point when);

    // Return the sum of all the amounts in recent history.
    int32_t Sum() const;

   private:
    // The amount of time represented by each bucket.
    const Clock::duration bucket_duration_;

    // The beginning of the oldest bucket in the recent-history time window, the
    // one pointed to by |tail_|.
    Clock::time_point begin_time_;

    // Ring buffer tracking the accumulated amount for each bucket.
    int32_t ring_of_buckets_[kNumBuckets]{};

    // The index of the oldest bucket in |ring_of_buckets_|. This can also be
    // thought of, equivalently, as the index just after the youngest bucket.
    index_mod_256_t tail_ = 0;
  };

  // The maximum number of packet sends that could be attempted during the
  // recent-history time window.
  const int max_packets_per_history_window_;

  // The range of time being tracked.
  const Clock::duration history_window_;

  // History tracking for send attempts, and success feeback. These timeseries
  // are in terms of when packets have left the Sender.
  FlowTracker burst_history_;
  FlowTracker feedback_history_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_BANDWIDTH_ESTIMATOR_H_
