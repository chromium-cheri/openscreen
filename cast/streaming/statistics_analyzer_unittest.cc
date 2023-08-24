// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/statistics_analyzer.h"

#include "cast/streaming/statistics.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"
#include "util/chrono_helpers.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::Return;
using testing::Sequence;
using testing::StrictMock;

namespace openscreen {
namespace cast {

namespace {

void ExpectStat(SenderStats::StatisticsList stats_list,
                StatisticType stat,
                double expected_value) {
  EXPECT_EQ(stats_list[static_cast<int>(stat)], expected_value);
}

class FakeSenderStatsClient : public SenderStatsClient {
 public:
  MOCK_METHOD(void, OnStatisticsUpdated, (const SenderStats&), (override));
};

}  // namespace

class StatisticsAnalyzerTest : public ::testing::Test {
 public:
  StatisticsAnalyzerTest()
      : fake_clock_(Clock::now()), fake_task_runner_(&fake_clock_) {}

  void SetUp() {
    analyzer_ = std::make_unique<StatisticsAnalyzer>(
        &stats_client_, fake_clock_.now, fake_task_runner_);
    collector_ = analyzer_->statistics_collector();
  }

 protected:
  StrictMock<FakeSenderStatsClient> stats_client_;
  FakeClock fake_clock_;
  FakeTaskRunner fake_task_runner_;
  std::unique_ptr<StatisticsAnalyzer> analyzer_;
  StatisticsCollector* collector_ = nullptr;
};

TEST_F(StatisticsAnalyzerTest, Encode) {
  analyzer_->ScheduleAnalysis();

  const int num_events = 20;
  const int analysis_interval_ms = 500;
  const int frame_interval_ms = 5;
  const int size_bytes = 10;

  Clock::time_point first_event_time = fake_clock_.now();
  Clock::time_point last_event_time;
  RtpTimeTicks rtp_timestamp;

  for (int i = 0; i < num_events; i++) {
    const FrameEvent event{FrameId(i),
                           StatisticsEventType::kFrameEncoded,
                           StatisticsEventMediaType::kVideo,
                           rtp_timestamp,
                           /* width= */ 640,
                           /* height= */ 480,
                           /* size= */ size_bytes,
                           /* timestamp */ fake_clock_.now(),
                           /* delay_delta= */ std::chrono::milliseconds(20),
                           /* key_frame=*/false,
                           0};

    collector_->CollectFrameEvent(event);
    last_event_time = fake_clock_.now();
    fake_clock_.Advance(std::chrono::milliseconds(5));
    rtp_timestamp += RtpTimeDelta::FromTicks(90);
  }

  EXPECT_CALL(stats_client_, OnStatisticsUpdated(_))
      .WillOnce(Invoke([&](const SenderStats& stats) {
        double expected_fps = num_events / (analysis_interval_ms / 1000.0);
        ExpectStat(stats.video_statistics, StatisticType::kEnqueueFps,
                   expected_fps);

        double expected_kbps = size_bytes * 8 * num_events /
                               static_cast<double>(analysis_interval_ms);
        ExpectStat(stats.video_statistics, StatisticType::kEncodeRateKbps,
                   expected_kbps);

        ExpectStat(
            stats.video_statistics, StatisticType::kFirstEventTimeMs,
            static_cast<double>(
                to_milliseconds(first_event_time.time_since_epoch()).count()));
        ExpectStat(
            stats.video_statistics, StatisticType::kLastEventTimeMs,
            static_cast<double>(
                to_milliseconds(last_event_time.time_since_epoch()).count()));
      }));

  fake_clock_.Advance(std::chrono::milliseconds(
      analysis_interval_ms - (frame_interval_ms * num_events)));
}

TEST_F(StatisticsAnalyzerTest, EncodeAndAckSent) {
  analyzer_->ScheduleAnalysis();

  const int num_events = 20;
  const int analysis_interval_ms = 500;
  const int frame_interval_ms = 5;
  const int size_bytes = 10;

  Clock::time_point first_event_time = fake_clock_.now();
  Clock::time_point last_event_time;
  RtpTimeTicks rtp_timestamp;

  for (int i = 0; i < num_events; i++) {
    const FrameEvent event1{FrameId(i),
                            StatisticsEventType::kFrameEncoded,
                            StatisticsEventMediaType::kVideo,
                            rtp_timestamp,
                            /* width= */ 640,
                            /* height= */ 480,
                            /* size= */ size_bytes,
                            /* timestamp */ fake_clock_.now(),
                            /* delay_delta= */ std::chrono::milliseconds(20),
                            /* key_frame=*/false,
                            0};

    const FrameEvent event2{FrameId(i),
                            StatisticsEventType::kFrameAckSent,
                            StatisticsEventMediaType::kVideo,
                            rtp_timestamp,
                            /* width= */ 640,
                            /* height= */ 480,
                            /* size= */ size_bytes,
                            /* timestamp */ fake_clock_.now(),
                            /* delay_delta= */ std::chrono::milliseconds(20),
                            /* key_frame=*/false,
                            0};

    collector_->CollectFrameEvent(event1);
    collector_->CollectFrameEvent(event2);
    last_event_time = fake_clock_.now();
    fake_clock_.Advance(std::chrono::milliseconds(5));
    rtp_timestamp += RtpTimeDelta::FromTicks(90);
  }

  // We should be able to test for the StatisticType::kAvgFrameLatencyMs, which
  // is encode time until frame ack sent time.
}

}  // namespace cast
}  // namespace openscreen
