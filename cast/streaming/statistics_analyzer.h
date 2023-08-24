// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_STATISTICS_ANALYZER_H_
#define CAST_STREAMING_STATISTICS_ANALYZER_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "cast/streaming/statistics.h"
#include "cast/streaming/statistics_collector.h"
#include "platform/api/time.h"
#include "util/alarm.h"

namespace openscreen {
namespace cast {

class StatisticsAnalyzer {
 public:
  StatisticsAnalyzer(SenderStatsClient* stats_client,
                     ClockNowFunctionPtr now,
                     TaskRunner& task_runner);
  ~StatisticsAnalyzer();

  void ScheduleAnalysis();

  // Get the statistics collector managed by this analyzer.
  StatisticsCollector* statistics_collector() {
    return statistics_collector_.get();
  }

 private:
  struct FrameStatsAggregate {
    FrameStatsAggregate();
    ~FrameStatsAggregate();
    int event_counter;
    uint32_t sum_size;
    Clock::duration sum_delay;
  };

  struct PacketStatsAggregate {
    PacketStatsAggregate();
    ~PacketStatsAggregate();
    int event_counter;
    uint32_t sum_size;
  };

  struct LatencyStatsAggregate {
    LatencyStatsAggregate();
    ~LatencyStatsAggregate();
    int data_point_counter;
    Clock::duration sum_latency;
  };

  struct FrameInfo {
    FrameInfo();
    ~FrameInfo();
    Clock::time_point encode_end_time;
  };

  struct PacketInfo {
    PacketInfo();
    ~PacketInfo();
    Clock::time_point timestamp;
    StatisticsEventType type;
  };

  struct SessionStats {
    SessionStats();
    ~SessionStats();
    Clock::time_point first_event_time = Clock::time_point::min();
    Clock::time_point last_event_time = Clock::time_point::min();
    Clock::time_point last_response_received_time = Clock::time_point::min();
    int late_frame_counter = 0;
  };

  typedef std::map<StatisticsEventType, FrameStatsAggregate> FrameStatsMap;
  typedef std::map<StatisticsEventType, PacketStatsAggregate> PacketStatsMap;
  typedef std::map<StatisticType, LatencyStatsAggregate> LatencyStatsMap;

  typedef std::pair<RtpTimeTicks, StatisticsEventMediaType> FrameKey;
  typedef std::pair<RtpTimeTicks, uint16_t> PacketKey;
  typedef std::map<FrameKey, FrameInfo> FrameInfoMap;
  typedef std::map<PacketKey, PacketInfo> PacketInfoMap;

  void InitHistograms();
  void AnalyzeStatistics();
  void SendStatistics();
  SenderStats::HistogramsList GetAudioHistograms();
  SenderStats::HistogramsList GetVideoHistograms();
  void ProcessFrameEvents(std::vector<FrameEvent> frame_events);
  void ProcessPacketEvents(std::vector<PacketEvent> packet_events);
  void RecordFrameLatencies(FrameEvent frame_event);
  void RecordPacketLatencies(PacketEvent packet_event);
  void RecordEventTimes(Clock::time_point timestamp,
                        StatisticsEventMediaType media_type,
                        bool is_receiver_event);
  void ErasePacketInfo(PacketEvent packet_event);
  void AddToLatencyAggregrate(StatisticType latency_stat,
                              Clock::duration latency_delta,
                              StatisticsEventMediaType media_type);
  void AddToHistogram(HistogramType histogram,
                      StatisticsEventMediaType media_type,
                      int64_t sample);
  FrameStatsMap* GetFrameStatsMapForMediaType(
      StatisticsEventMediaType media_type);
  PacketStatsMap* GetPacketStatsMapForMediaType(
      StatisticsEventMediaType media_type);
  LatencyStatsMap* GetLatencyStatsMapForMediaType(
      StatisticsEventMediaType media_type);
  SessionStats* GetSessionStatsForMediaType(
      StatisticsEventMediaType media_type);
  SenderStats::StatisticsList ConstructStatisticsList(
      Clock::time_point end_time,
      StatisticsEventMediaType media_type);
  SenderStats::StatisticsList PopulateFrameCountStat(
      StatisticsEventType event,
      StatisticType stat,
      SenderStats::StatisticsList stats_list,
      StatisticsEventMediaType media_type);
  SenderStats::StatisticsList PopulatePacketCountStat(
      StatisticsEventType event,
      StatisticType stat,
      SenderStats::StatisticsList stats_list,
      StatisticsEventMediaType media_type);
  SenderStats::StatisticsList PopulateFpsStat(
      StatisticsEventType event,
      StatisticType stat,
      SenderStats::StatisticsList stats_list,
      StatisticsEventMediaType media_type,
      Clock::time_point end_time);
  SenderStats::StatisticsList PopulateAvgLatencyStat(
      StatisticType stat,
      SenderStats::StatisticsList stats_list,
      StatisticsEventMediaType media_type);
  SenderStats::StatisticsList PopulateFrameBitrateStat(
      StatisticsEventType event,
      StatisticType stat,
      SenderStats::StatisticsList stats_list,
      StatisticsEventMediaType media_type,
      Clock::time_point end_time);
  SenderStats::StatisticsList PopulatePacketBitrateStat(
      StatisticsEventType event,
      StatisticType stat,
      SenderStats::StatisticsList stats_list,
      StatisticsEventMediaType media_type,
      Clock::time_point end_time);
  SenderStats::StatisticsList PopulateSessionStats(
      SenderStats::StatisticsList stats_list,
      StatisticsEventMediaType media_type,
      Clock::time_point end_time);

  // The statistics client to which we report analyzed statistics.
  SenderStatsClient* stats_client_;

  ClockNowFunctionPtr now_;

  Alarm alarm_;

  FrameInfoMap recent_frame_infos_;
  PacketInfoMap recent_packet_infos_;

  FrameStatsMap audio_frame_stats_;
  FrameStatsMap video_frame_stats_;
  PacketStatsMap audio_packet_stats_;
  PacketStatsMap video_packet_stats_;

  LatencyStatsMap audio_latency_stats_;
  LatencyStatsMap video_latency_stats_;

  // The statistics collector from which we take the un-analyzed stats packets.
  std::unique_ptr<StatisticsCollector> statistics_collector_;

  Clock::time_point start_time_;

  SessionStats audio_session_stats_;
  SessionStats video_session_stats_;

  // Histograms
  SenderStats::HistogramsList audio_histograms_;
  SenderStats::HistogramsList video_histograms_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_STATISTICS_ANALYZER_H_
