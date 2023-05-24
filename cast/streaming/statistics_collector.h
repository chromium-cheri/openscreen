// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_STATISTICS_COLLECTOR_H_
#define CAST_STREAMING_STATISTICS_COLLECTOR_H_

#include <vector>

#include "cast/streaming/statistics_defines.h"
#include "platform/api/time.h"
#include "platform/base/span.h"

namespace openscreen {
namespace cast {

class StatisticsCollector {
 public:
  explicit StatisticsCollector(ClockNowFunctionPtr now);
  ~StatisticsCollector();

  void CollectPacketSent(ByteView packet);
  void CollectPacketEvent(const PacketEvent& event);
  void CollectFrameEvent(const FrameEvent& event);

 private:
  ClockNowFunctionPtr now_;
  std::vector<PacketEvent> recent_packet_events_;
  std::vector<FrameEvent> recent_frame_events_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_STATISTICS_COLLECTOR_H_
