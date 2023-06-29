// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/statistics_collector.h"

#include <vector>

#include "cast/streaming/statistics_defines.h"
#include "gtest/gtest.h"
#include "platform/api/time.h"
#include "platform/base/span.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"

namespace openscreen {
namespace cast {

class StatisticsCollectorTest : public ::testing::Test {
 public:
  StatisticsCollectorTest()
      : fake_clock_(Clock::now()), collector_(fake_clock_.now) {}

 protected:
  FakeClock fake_clock_;
  StatisticsCollector collector_;
};

TEST_F(StatisticsCollectorTest, ReturnsEmptyIfNoEvents) {
  EXPECT_TRUE(collector_.TakeRecentPacketEvents().empty());
  EXPECT_TRUE(collector_.TakeRecentFrameEvents().empty());
}

TEST_F(StatisticsCollectorTest, CanCollectPacketEvents) {}

TEST_F(StatisticsCollectorTest, CanCollectPacketSentEvents) {}

TEST_F(StatisticsCollectorTest, CanCollectFrameEvents) {}

}  // namespace cast
}  // namespace openscreen
