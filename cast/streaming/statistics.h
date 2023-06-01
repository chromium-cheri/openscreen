// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_STATISTICS_H_
#define CAST_STREAMING_STATISTICS_H_

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "cast/streaming/frame_id.h"
#include "cast/streaming/rtp_time.h"
#include "platform/api/time.h"
#include "util/enum_name_table.h"

namespace openscreen {
namespace cast {

enum class StatisticType : int {
  // Frame enqueuing rate.
  kEnqueueFps,

  // TODO(https://issuetracker.google.com/285417419): providable through this
  // API? support TBD.
  // Decode frame rate.
  kDecodeFps,

  // TODO(https://issuetracker.google.com/285417419): providable through this
  // API? support TBD.
  // Average capture latency in milliseconds.
  kAvgCaptureLatencyMs,

  // TODO(https://issuetracker.google.com/285417419): providable through this
  // API? support TBD.
  // Average encode duration in milliseconds.
  kAvgEncodeTimeMs,

  // Duration from when a frame is encoded to when the packet is first
  // sent.
  kAvgQueueingLatencyMs,

  // Duration from when a packet is transmitted to when it is received.
  // This measures latency from sender to receiver.
  kAvgNetworkLatencyMs,

  // Duration from when a frame is encoded to when the packet is first
  // received.
  kAvgPacketLatencyMs,

  // Average latency between frame encoded and the moment when the frame
  // is fully received.
  kAvgFrameLatencyMs,

  // Duration from when a frame is captured to when it should be played out.
  kAvgEndToEndLatencyMs,

  // TODO(https://issuetracker.google.com/285417419): providable through this
  // API? support TBD.
  // Encode bitrate in kbps.
  kEncodeRateKbps,

  // Packet transmission bitrate in kbps.
  kPacketTransmissionRateKbps,

  // Duration in milliseconds since last receiver response.
  kTimeSinceLastReceiverResponseMs,

  // Number of frames captured.
  kNumFramesCaptured,

  // Number of frames dropped by encoder.
  kNumFramesDroppedByEncoder,

  // Number of late frames.
  kNumLateFrames,

  // Number of packets that were sent.
  kNumPacketsSent,

  // Number of packets that were received by receiver.
  kNumPacketsReceived,

  // Unix time in milliseconds of first event since reset.
  kFirstEventTimeMs,

  // Unix time in milliseconds of last event since reset.
  kLastEventTimeMs
};

enum class HistogramType : int {
  kCaptureLatency,
  kEncodeTime,
  kQueueingLatency,
  kNetworkLatency,
  kPacketLatency,
  kEndToEndLatency,
  kFrameLateness
};

struct SimpleHistogram {
 public:
  // This will create N+2 buckets where N = (max - min) / width:
  // Underflow bucket: < min
  // Bucket 0: [min, min + width - 1]
  // Bucket 1: [min + width, min + 2 * width - 1]
  // ...
  // Bucket N-1: [max - width, max - 1]
  // Overflow bucket: >= max
  // |min| must be less than |max|.
  // |width| must divide |max - min| evenly.
  SimpleHistogram(int64_t min, int64_t max, int64_t width);
  ~SimpleHistogram();

  void Add(int64_t sample);
  void Reset();

  int64_t min() const { return min_; }
  int64_t max() const { return max_; }
  int64_t width() const { return width_; }
  const std::vector<int>& buckets() const { return buckets_; }

 private:
  int64_t min_;
  int64_t max_;
  int64_t width_;
  std::vector<int> buckets_;
};

struct Statistics {
  using StatisticsList = std::vector<std::pair<StatisticType, double>>;
  using HistogramsList = std::vector<std::pair<HistogramType, SimpleHistogram>>;

  // The current audio statistics.
  StatisticsList audio_statistics;

  // The current audio histograms.
  HistogramsList audio_histograms;

  // The current video statistics.
  StatisticsList video_statistics;

  // The current video histograms.
  HistogramsList video_histograms;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_STATISTICS_H_
