// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/statistics_analyzer.h"

#include <numeric>

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
  EXPECT_DOUBLE_EQ(stats_list[static_cast<int>(stat)], expected_value);
}

// Checks that the first `expected_buckets.size()` entries of `recorded_buckets`
// matches the entries of `expected_buckets`. Also, checks that the total number
// of events matches for both vector.s
void ExpectHisto(std::vector<int> recorded_buckets,
                 std::vector<int> expected_buckets) {
  for (size_t i = 0; i < expected_buckets.size(); i++) {
    EXPECT_EQ(recorded_buckets[i], expected_buckets[i]);
  }
  int total_recorded_events =
      std::accumulate(recorded_buckets.begin(), recorded_buckets.end(), 0);
  int total_expected_events =
      std::accumulate(expected_buckets.begin(), expected_buckets.end(), 0);
  EXPECT_EQ(total_recorded_events, total_expected_events);
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

TEST_F(StatisticsAnalyzerTest, FrameEncoded) {
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
    fake_clock_.Advance(std::chrono::milliseconds(frame_interval_ms));
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

TEST_F(StatisticsAnalyzerTest, FrameEncodedAndAckSent) {
  analyzer_->ScheduleAnalysis();

  const int num_events = 20;
  const int analysis_interval_ms = 500;
  const int frame_interval_ms = 5;
  const int size_bytes = 10;

  Clock::duration total_frame_latency = std::chrono::milliseconds(0);
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

    // Let random frame delay be anywhere from 20 - 39 ms
    Clock::duration random_latency =
        std::chrono::milliseconds(20 + (rand() % 20));
    total_frame_latency += random_latency;

    const FrameEvent event2{FrameId(i),
                            StatisticsEventType::kFrameAckSent,
                            StatisticsEventMediaType::kVideo,
                            rtp_timestamp,
                            /* width= */ 640,
                            /* height= */ 480,
                            /* size= */ size_bytes,
                            /* timestamp */ fake_clock_.now() + random_latency,
                            /* delay_delta= */ std::chrono::milliseconds(20),
                            /* key_frame=*/false,
                            0};

    collector_->CollectFrameEvent(event1);
    collector_->CollectFrameEvent(event2);
    fake_clock_.Advance(std::chrono::milliseconds(frame_interval_ms));
    rtp_timestamp += RtpTimeDelta::FromTicks(90);
  }

  EXPECT_CALL(stats_client_, OnStatisticsUpdated(_))
      .WillOnce(Invoke([&](const SenderStats& stats) {
        double expected_avg_frame_latency =
            static_cast<double>(to_milliseconds(total_frame_latency).count()) /
            num_events;
        ExpectStat(stats.video_statistics, StatisticType::kAvgFrameLatencyMs,
                   expected_avg_frame_latency);
      }));

  fake_clock_.Advance(std::chrono::milliseconds(
      analysis_interval_ms - (frame_interval_ms * num_events)));
}

TEST_F(StatisticsAnalyzerTest, FramePlayedOut) {
  analyzer_->ScheduleAnalysis();

  const int num_events = 20;
  const int analysis_interval_ms = 500;
  const int frame_interval_ms = 5;
  const int size_bytes = 10;

  RtpTimeTicks rtp_timestamp;
  int total_late_frames = 0;

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

    // Let random frame delay be anywhere from 20 - 39 ms.
    Clock::duration random_latency =
        std::chrono::milliseconds(20 + (rand() % 20));

    // Frames will have delay_deltas of -20, 0, 20, 40, or 60 ms.
    auto delay_delta = std::chrono::milliseconds(60 - (20 * (i % 5)));

    const FrameEvent event2{FrameId(i),
                            StatisticsEventType::kFramePlayedOut,
                            StatisticsEventMediaType::kVideo,
                            rtp_timestamp,
                            /* width= */ 640,
                            /* height= */ 480,
                            /* size= */ size_bytes,
                            /* timestamp */ fake_clock_.now() + random_latency,
                            /* delay_delta= */ delay_delta,
                            /* key_frame=*/false,
                            0};
    if (delay_delta > std::chrono::milliseconds(0)) {
      total_late_frames++;
    }

    collector_->CollectFrameEvent(event1);
    collector_->CollectFrameEvent(event2);
    fake_clock_.Advance(std::chrono::milliseconds(frame_interval_ms));
    rtp_timestamp += RtpTimeDelta::FromTicks(90);
  }

  EXPECT_CALL(stats_client_, OnStatisticsUpdated(_))
      .WillOnce(Invoke([&](const SenderStats& stats) {
        ExpectStat(stats.video_statistics, StatisticType::kNumLateFrames,
                   total_late_frames);

        std::vector<int> expected_buckets = {/* <0 */ 0,
                                             /* 0-19 */ 0,
                                             /* 20-39 */ 4,
                                             /* 40-59 */ 4,
                                             /* 60-79 */ 4,
                                             /* 80-99 */ 0};
        std::vector<int> recorded_buckets =
            stats
                .video_histograms[static_cast<int>(
                    HistogramType::kFrameLatenessMs)]
                .buckets;
        ExpectHisto(recorded_buckets, expected_buckets);
      }));

  fake_clock_.Advance(std::chrono::milliseconds(
      analysis_interval_ms - (frame_interval_ms * num_events)));
}

TEST_F(StatisticsAnalyzerTest, FrameEncodedAndPacketSent) {
  analyzer_->ScheduleAnalysis();

  const int num_events = 20;
  const int analysis_interval_ms = 500;
  const int frame_interval_ms = 5;
  const int size_bytes = 10;

  Clock::duration total_queueing_latency = std::chrono::milliseconds(0);
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

    // Let queueing latency be either 0, 20, 40, 60, or 80 ms.
    Clock::duration queueing_latency =
        std::chrono::milliseconds(80 - (20 * (i % 5)));
    total_queueing_latency += queueing_latency;

    const PacketEvent event2{static_cast<uint16_t>(i),
                             50u,
                             rtp_timestamp,
                             FrameId(i),
                             size_bytes,
                             fake_clock_.now() + queueing_latency,
                             StatisticsEventType::kPacketSentToNetwork,
                             StatisticsEventMediaType::kVideo};

    collector_->CollectFrameEvent(event1);
    collector_->CollectPacketEvent(event2);
    fake_clock_.Advance(std::chrono::milliseconds(frame_interval_ms));
    rtp_timestamp += RtpTimeDelta::FromTicks(90);
  }

  EXPECT_CALL(stats_client_, OnStatisticsUpdated(_))
      .WillOnce(Invoke([&](const SenderStats& stats) {
        double expected_kbps = size_bytes * 8 * num_events /
                               static_cast<double>(analysis_interval_ms);
        ExpectStat(stats.video_statistics,
                   StatisticType::kPacketTransmissionRateKbps, expected_kbps);

        double expected_avg_queueing_latency =
            static_cast<double>(
                to_milliseconds(total_queueing_latency).count()) /
            num_events;
        ExpectStat(stats.video_statistics, StatisticType::kAvgQueueingLatencyMs,
                   expected_avg_queueing_latency);

        std::vector<int> expected_buckets = {/* <0 */ 0,
                                             /* 0-19 */ 4,
                                             /* 20-39 */ 4,
                                             /* 40-59 */ 4,
                                             /* 60-79 */ 4,
                                             /* 80-99 */ 4,
                                             /* 100-119 */ 0};
        std::vector<int> recorded_buckets =
            stats
                .video_histograms[static_cast<int>(
                    HistogramType::kQueueingLatencyMs)]
                .buckets;
        ExpectHisto(recorded_buckets, expected_buckets);
      }));

  fake_clock_.Advance(std::chrono::milliseconds(
      analysis_interval_ms - (frame_interval_ms * num_events)));
}

TEST_F(StatisticsAnalyzerTest, PacketSentAndReceived) {
  analyzer_->ScheduleAnalysis();

  const int num_events = 20;
  const int analysis_interval_ms = 500;
  const int frame_interval_ms = 5;
  const int size_bytes = 10;

  Clock::duration total_network_latency = std::chrono::milliseconds(0);
  RtpTimeTicks rtp_timestamp;

  for (int i = 0; i < num_events; i++) {
    const PacketEvent event1{static_cast<uint16_t>(i),
                             50u,
                             rtp_timestamp,
                             FrameId(i),
                             size_bytes,
                             fake_clock_.now(),
                             StatisticsEventType::kPacketSentToNetwork,
                             StatisticsEventMediaType::kVideo};

    // Let network latency be either 0, 20, 40, 60, or 80 ms.
    Clock::duration network_latency =
        std::chrono::milliseconds(80 - (20 * (i % 5)));
    total_network_latency += network_latency;

    const PacketEvent event2{static_cast<uint16_t>(i),
                             50u,
                             rtp_timestamp,
                             FrameId(i),
                             size_bytes,
                             fake_clock_.now() + network_latency,
                             StatisticsEventType::kPacketReceived,
                             StatisticsEventMediaType::kVideo};

    collector_->CollectPacketEvent(event1);
    collector_->CollectPacketEvent(event2);
    fake_clock_.Advance(std::chrono::milliseconds(frame_interval_ms));
    rtp_timestamp += RtpTimeDelta::FromTicks(90);
  }

  EXPECT_CALL(stats_client_, OnStatisticsUpdated(_))
      .WillOnce(Invoke([&](const SenderStats& stats) {
        double expected_avg_network_latency =
            static_cast<double>(
                to_milliseconds(total_network_latency).count()) /
            num_events;
        ExpectStat(stats.video_statistics, StatisticType::kAvgNetworkLatencyMs,
                   expected_avg_network_latency);

        std::vector<int> expected_buckets = {/* <0 */ 0,
                                             /* 0-19 */ 4,
                                             /* 20-39 */ 4,
                                             /* 40-59 */ 4,
                                             /* 60-79 */ 4,
                                             /* 80-99 */ 4,
                                             /* 100-119 */ 0};
        std::vector<int> recorded_buckets =
            stats
                .video_histograms[static_cast<int>(
                    HistogramType::kNetworkLatencyMs)]
                .buckets;
        ExpectHisto(recorded_buckets, expected_buckets);
      }));

  fake_clock_.Advance(std::chrono::milliseconds(
      analysis_interval_ms - (frame_interval_ms * num_events)));
}

TEST_F(StatisticsAnalyzerTest, FrameEncodedPacketSentAndReceived) {
  analyzer_->ScheduleAnalysis();

  const int num_events = 20;
  const int analysis_interval_ms = 500;
  const int frame_interval_ms = 5;
  const int size_bytes = 10;

  Clock::duration total_packet_latency = std::chrono::milliseconds(0);
  RtpTimeTicks rtp_timestamp;
  Clock::time_point last_event_time;

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

    const PacketEvent event2{static_cast<uint16_t>(i),
                             50u,
                             rtp_timestamp,
                             FrameId(i),
                             size_bytes,
                             fake_clock_.now(),
                             StatisticsEventType::kPacketSentToNetwork,
                             StatisticsEventMediaType::kVideo};

    // Let packet latency be either 20, 40, 60, 80, or 100 ms.
    Clock::duration packet_latency =
        std::chrono::milliseconds(100 - (20 * (i % 5)));
    total_packet_latency += packet_latency;
    if (fake_clock_.now() + packet_latency > last_event_time) {
      last_event_time = fake_clock_.now() + packet_latency;
    }

    const PacketEvent event3{static_cast<uint16_t>(i),
                             50u,
                             rtp_timestamp,
                             FrameId(i),
                             size_bytes,
                             fake_clock_.now() + packet_latency,
                             StatisticsEventType::kPacketReceived,
                             StatisticsEventMediaType::kVideo};

    collector_->CollectFrameEvent(event1);
    collector_->CollectPacketEvent(event2);
    collector_->CollectPacketEvent(event3);
    fake_clock_.Advance(std::chrono::milliseconds(frame_interval_ms));
    rtp_timestamp += RtpTimeDelta::FromTicks(90);
  }

  EXPECT_CALL(stats_client_, OnStatisticsUpdated(_))
      .WillOnce(Invoke([&](const SenderStats& stats) {
        ExpectStat(stats.video_statistics, StatisticType::kNumPacketsSent,
                   num_events);
        ExpectStat(stats.video_statistics, StatisticType::kNumPacketsReceived,
                   num_events);

        double expected_time_since_last_receiver_response = static_cast<double>(
            to_milliseconds(fake_clock_.now() - last_event_time).count());
        ExpectStat(stats.video_statistics,
                   StatisticType::kTimeSinceLastReceiverResponseMs,
                   expected_time_since_last_receiver_response);

        double expected_avg_packet_latency =
            static_cast<double>(to_milliseconds(total_packet_latency).count()) /
            num_events;
        ExpectStat(stats.video_statistics, StatisticType::kAvgPacketLatencyMs,
                   expected_avg_packet_latency);

        std::vector<int> expected_buckets = {/* <0 */ 0,
                                             /* 0-19 */ 0,
                                             /* 20-39 */ 4,
                                             /* 40-59 */ 4,
                                             /* 60-79 */ 4,
                                             /* 80-99 */ 4,
                                             /* 100-119 */ 4,
                                             /* 120-139 */ 0};
        std::vector<int> recorded_buckets =
            stats
                .video_histograms[static_cast<int>(
                    HistogramType::kPacketLatencyMs)]
                .buckets;
        ExpectHisto(recorded_buckets, expected_buckets);
      }));

  fake_clock_.Advance(std::chrono::milliseconds(
      analysis_interval_ms - (frame_interval_ms * num_events)));
}

TEST_F(StatisticsAnalyzerTest, AudioAndVideoFrameEncodedPacketSentAndReceived) {
  analyzer_->ScheduleAnalysis();

  const int num_events = 100;
  const int analysis_interval_ms = 500;
  const int frame_interval_ms = 2;
  const int size_bytes = 10;

  RtpTimeTicks rtp_timestamp;
  Clock::duration total_audio_packet_latency = std::chrono::milliseconds(0);
  Clock::duration total_video_packet_latency = std::chrono::milliseconds(0);
  int total_audio_events = 0;
  int total_video_events = 0;

  for (int i = 0; i < num_events; i++) {
    StatisticsEventMediaType media_type = StatisticsEventMediaType::kVideo;
    if (i % 2 == 0) {
      media_type = StatisticsEventMediaType::kAudio;
    }
    const FrameEvent event1{FrameId(i),
                            StatisticsEventType::kFrameEncoded,
                            media_type,
                            rtp_timestamp,
                            /* width= */ 640,
                            /* height= */ 480,
                            /* size= */ size_bytes,
                            /* timestamp */ fake_clock_.now(),
                            /* delay_delta= */ std::chrono::milliseconds(20),
                            /* key_frame=*/false,
                            0};

    const PacketEvent event2{static_cast<uint16_t>(i),
                             50u,
                             rtp_timestamp,
                             FrameId(i),
                             size_bytes,
                             fake_clock_.now() + std::chrono::milliseconds(5),
                             StatisticsEventType::kPacketSentToNetwork,
                             media_type};

    // Let packet latency be either 20, 40, 60, 80, or 100 ms.
    Clock::duration packet_latency =
        std::chrono::milliseconds(100 - (20 * (i % 5)));
    if (media_type == StatisticsEventMediaType::kAudio) {
      total_audio_events++;
      total_audio_packet_latency += packet_latency;
    } else if (media_type == StatisticsEventMediaType::kVideo) {
      total_video_events++;
      total_video_packet_latency += packet_latency;
    }

    const PacketEvent event3{static_cast<uint16_t>(i),
                             50u,
                             rtp_timestamp,
                             FrameId(i),
                             size_bytes,
                             fake_clock_.now() + packet_latency,
                             StatisticsEventType::kPacketReceived,
                             media_type};

    collector_->CollectFrameEvent(event1);
    collector_->CollectPacketEvent(event2);
    collector_->CollectPacketEvent(event3);
    fake_clock_.Advance(std::chrono::milliseconds(frame_interval_ms));
    rtp_timestamp += RtpTimeDelta::FromTicks(90);
  }

  EXPECT_CALL(stats_client_, OnStatisticsUpdated(_))
      .WillOnce(Invoke([&](const SenderStats& stats) {
        ExpectStat(stats.audio_statistics, StatisticType::kNumPacketsSent,
                   total_audio_events);
        ExpectStat(stats.audio_statistics, StatisticType::kNumPacketsReceived,
                   total_audio_events);
        ExpectStat(stats.video_statistics, StatisticType::kNumPacketsSent,
                   total_video_events);
        ExpectStat(stats.video_statistics, StatisticType::kNumPacketsReceived,
                   total_video_events);

        double expected_audio_avg_packet_latency =
            static_cast<double>(
                to_milliseconds(total_audio_packet_latency).count()) /
            total_audio_events;
        ExpectStat(stats.audio_statistics, StatisticType::kAvgPacketLatencyMs,
                   expected_audio_avg_packet_latency);
        double expected_video_avg_packet_latency =
            static_cast<double>(
                to_milliseconds(total_video_packet_latency).count()) /
            total_video_events;
        ExpectStat(stats.video_statistics, StatisticType::kAvgPacketLatencyMs,
                   expected_video_avg_packet_latency);
      }));

  fake_clock_.Advance(std::chrono::milliseconds(
      analysis_interval_ms - (frame_interval_ms * num_events)));
}

}  // namespace cast
}  // namespace openscreen
