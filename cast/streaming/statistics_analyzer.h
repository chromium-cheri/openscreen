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
    FrameStatsAggregate() = default;
    ~FrameStatsAggregate() = default;
    int event_counter;
    uint32_t sum_size;
    Clock::duration sum_delay;
  };

  struct PacketStatsAggregate {
    PacketStatsAggregate() = default;
    ~PacketStatsAggregate() = default;
    int event_counter;
    uint32_t sum_size;
  };

  struct LatencyStatsAggregate {
    LatencyStatsAggregate() = default;
    ~LatencyStatsAggregate() = default;
    int data_point_counter;
    Clock::duration sum_latency;
  };

  struct FrameInfo {
    FrameInfo() = default;
    ~FrameInfo() = default;
    Clock::time_point encode_end_time;
  };

  struct PacketInfo {
    PacketInfo() = default;
    ~PacketInfo() = default;
    Clock::time_point timestamp;
    StatisticsEventType type;
  };

  struct SessionStats {
    SessionStats() = default;
    ~SessionStats() = default;
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

  // Initialize the stats histograms with the preferred min, max, and width.
  void InitHistograms();

  // Takes the Frame and Packet events from the `collector_`, and processes them
  // into a form expected by `stats_client_`. Then sends the stats, and
  // schedules a future analysis.
  void AnalyzeStatistics();

  // Constructs a stats list, and sends it to `stats_client_`;
  void SendStatistics();

  // Handles incoming stat events, and adds their infos to all of the proper
  // stats maps / aggregates.
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

  // Gets a reference to the appropriate object based on `media_type`.
  FrameStatsMap* GetFrameStatsMapForMediaType(
      StatisticsEventMediaType media_type);
  PacketStatsMap* GetPacketStatsMapForMediaType(
      StatisticsEventMediaType media_type);
  LatencyStatsMap* GetLatencyStatsMapForMediaType(
      StatisticsEventMediaType media_type);
  SessionStats* GetSessionStatsForMediaType(
      StatisticsEventMediaType media_type);

  // Create copies of the stat histograms in their current stats, and return
  // them as a list.
  SenderStats::HistogramsList GetAudioHistograms();
  SenderStats::HistogramsList GetVideoHistograms();

  // Creates a stats list, and populates the entries based on stored stats info
  // / aggregates for each stat field.
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

  // The statistics collector from which we take the un-analyzed stats packets.
  std::unique_ptr<StatisticsCollector> statistics_collector_;

  // Keep track of time and events for this analyzer.
  ClockNowFunctionPtr now_;
  Alarm alarm_;
  Clock::time_point start_time_;

  // Maps of frame / packet infos used for stats that rely on seeing multiple
  // events. For example, network latency is the calculated time difference
  // between went a packet is sent, and when it is received.
  FrameInfoMap recent_frame_infos_;
  PacketInfoMap recent_packet_infos_;

  // Aggregate stats for particular event types.
  FrameStatsMap audio_frame_stats_;
  FrameStatsMap video_frame_stats_;
  PacketStatsMap audio_packet_stats_;
  PacketStatsMap video_packet_stats_;

  // Aggregates related to latency-type stats.
  LatencyStatsMap audio_latency_stats_;
  LatencyStatsMap video_latency_stats_;

  // Stats that relate to the entirety of the session. For example, total late
  // frames, or time of last event.
  SessionStats audio_session_stats_;
  SessionStats video_session_stats_;

  // Histograms
  SenderStats::HistogramsList audio_histograms_;
  SenderStats::HistogramsList video_histograms_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_STATISTICS_ANALYZER_H_
