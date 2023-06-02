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

enum class StatisticType {
  // Frame enqueuing rate.
  kEnqueueFps = 0,

  // TODO(https://issuetracker.google.com/285417419): providable through this
  // API? support TBD.
  // Average capture latency in milliseconds.
  kAvgCaptureLatencyMs = 1,

  // Average encode duration in milliseconds.
  kAvgEncodeTimeMs = 2,

  // Duration from when a frame is encoded to when the packet is first
  // sent.
  kAvgQueueingLatencyMs = 3,

  // Duration from when a packet is transmitted to when it is received.
  // This measures latency from sender to receiver.
  kAvgNetworkLatencyMs = 4,

  // Duration from when a frame is encoded to when the packet is first
  // received.
  kAvgPacketLatencyMs = 5,

  // Average latency between frame encoded and the moment when the frame
  // is fully received.
  kAvgFrameLatencyMs = 6,

  // Duration from when a frame is captured to when it should be played out.
  kAvgEndToEndLatencyMs = 7,

  // Encode bitrate in kbps.
  kEncodeRateKbps = 8,

  // Packet transmission bitrate in kbps.
  kPacketTransmissionRateKbps = 9,

  // Duration in milliseconds since last receiver response.
  kTimeSinceLastReceiverResponseMs = 10,

  // Number of frames captured.
  kNumFramesCaptured = 11,

  // Number of frames dropped by encoder.
  kNumFramesDroppedByEncoder = 12,

  // Number of late frames.
  kNumLateFrames = 13,

  // Number of packets that were sent.
  kNumPacketsSent = 14,

  // Number of packets that were received by receiver.
  kNumPacketsReceived = 15,

  // Unix time in milliseconds of first event since reset.
  kFirstEventTimeMs = 16,

  // Unix time in milliseconds of last event since reset.
  kLastEventTimeMs = 17,

  // The number of statistic types.
  kNumTypes = kLastEventTimeMs + 1
};

enum class HistogramType {
  // Histogram representing the capture latency (in milliseconds).
  kCaptureLatencyMs = 0,

  // Histogram representing the encode time (in milliseconds).
  kEncodeTimeMs = 1,

  // Histogram representing the queueing latency (in milliseconds).
  kQueueingLatencyMs = 2,

  // Histogram representing the network latency (in milliseconds).
  kNetworkLatencyMs = 3,

  // Histogram representing the packet latency (in milliseconds).
  kPacketLatencyMs = 4,

  // Histogram representing the end to end latency (in milliseconds).
  kEndToEndLatencyMs = 5,

  // Histogram representing how late frames are (in milliseconds).
  kFrameLatenessMs = 6,

  // The number of histogram types.
  kNumTypes = kFrameLatenessMs + 1
};

struct SimpleHistogram {
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

  int64_t min;
  int64_t max;
  int64_t width;
  std::vector<int> buckets;
};

struct SenderStats {
  using StatisticsList =
      std::array<std::pair<StatisticType, double>,
                 static_cast<size_t>(StatisticType::kNumTypes)>;
  using HistogramsList =
      std::array<std::pair<HistogramType, SimpleHistogram>,
                 static_cast<size_t>(HistogramType::kNumTypes)>;

  // The current audio statistics.
  StatisticsList audio_statistics;

  // The current audio histograms.
  HistogramsList audio_histograms;

  // The current video statistics.
  StatisticsList video_statistics;

  // The current video histograms.
  HistogramsList video_histograms;
};

// The consumer may provide a statistics client if they are interested in
// getting statistics about the ongoing session.
class SenderStatsClient {
  // Gets called regularly with updated statistics while they are being
  // generated.
  void OnStatisticsUpdated(const SenderStats& updated_stats);

 protected:
  virtual ~SenderStatsClient();
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_STATISTICS_H_
