// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/statistics_analyzer.h"

#include <algorithm>

#include "util/chrono_helpers.h"
#include "util/osp_logging.h"

double InMilliseconds(openscreen::Clock::duration duration) {
  return static_cast<double>(openscreen::to_milliseconds(duration).count());
}

bool IsReceiverEvent(openscreen::cast::StatisticsEventType event) {
  return event == openscreen::cast::StatisticsEventType::kFrameAckSent ||
         event == openscreen::cast::StatisticsEventType::kFrameDecoded ||
         event == openscreen::cast::StatisticsEventType::kFramePlayedOut ||
         event == openscreen::cast::StatisticsEventType::kPacketReceived;
}

namespace openscreen {
namespace cast {

constexpr Clock::duration kStatisticsAnalysisInverval =
    std::chrono::milliseconds(500);
const size_t kMaxRecentPacketInfoMapSize = 1000;
const size_t kMaxRecentFrameInfoMapSize = 200;

const int kDefaultMaxLatencyBucketMs = 800;
const int kDefaultBucketWidthMs = 20;

StatisticsAnalyzer::StatisticsAnalyzer(SenderStatsClient* stats_client,
                                       ClockNowFunctionPtr now,
                                       TaskRunner& task_runner)
    : stats_client_(stats_client),
      now_(now),
      alarm_(now, task_runner),
      start_time_(now()) {
  statistics_collector_ = std::make_unique<StatisticsCollector>(now_);
  InitHistograms();
}

StatisticsAnalyzer::~StatisticsAnalyzer() {
  statistics_collector_.reset();
}

void StatisticsAnalyzer::ScheduleAnalysis() {
  const Clock::time_point next_analysis_time =
      now_() + kStatisticsAnalysisInverval;
  alarm_.Schedule([this] { AnalyzeStatistics(); }, next_analysis_time);
}

void StatisticsAnalyzer::InitHistograms() {
  audio_histograms_[static_cast<int>(HistogramType::kQueueingLatencyMs)] =
      SimpleHistogram(0, kDefaultMaxLatencyBucketMs, kDefaultBucketWidthMs);
  audio_histograms_[static_cast<int>(HistogramType::kNetworkLatencyMs)] =
      SimpleHistogram(0, kDefaultMaxLatencyBucketMs, kDefaultBucketWidthMs);
  audio_histograms_[static_cast<int>(HistogramType::kPacketLatencyMs)] =
      SimpleHistogram(0, kDefaultMaxLatencyBucketMs, kDefaultBucketWidthMs);
  audio_histograms_[static_cast<int>(HistogramType::kFrameLatenessMs)] =
      SimpleHistogram(0, kDefaultMaxLatencyBucketMs, kDefaultBucketWidthMs);
  video_histograms_[static_cast<int>(HistogramType::kQueueingLatencyMs)] =
      SimpleHistogram(0, kDefaultMaxLatencyBucketMs, kDefaultBucketWidthMs);
  video_histograms_[static_cast<int>(HistogramType::kNetworkLatencyMs)] =
      SimpleHistogram(0, kDefaultMaxLatencyBucketMs, kDefaultBucketWidthMs);
  video_histograms_[static_cast<int>(HistogramType::kPacketLatencyMs)] =
      SimpleHistogram(0, kDefaultMaxLatencyBucketMs, kDefaultBucketWidthMs);
  video_histograms_[static_cast<int>(HistogramType::kFrameLatenessMs)] =
      SimpleHistogram(0, kDefaultMaxLatencyBucketMs, kDefaultBucketWidthMs);
}

void StatisticsAnalyzer::AnalyzeStatistics() {
  ProcessFrameEvents(statistics_collector_->TakeRecentFrameEvents());
  ProcessPacketEvents(statistics_collector_->TakeRecentPacketEvents());
  SendStatistics();
  ScheduleAnalysis();
}

void StatisticsAnalyzer::SendStatistics() {
  Clock::time_point end_time = now_();
  stats_client_->OnStatisticsUpdated(SenderStats{
      .audio_statistics =
          ConstructStatisticsList(end_time, StatisticsEventMediaType::kAudio),
      .audio_histograms = GetAudioHistograms(),
      .video_statistics =
          ConstructStatisticsList(end_time, StatisticsEventMediaType::kVideo),
      .video_histograms = GetVideoHistograms()});
}

void StatisticsAnalyzer::ProcessFrameEvents(
    std::vector<FrameEvent> frame_events) {
  for (FrameEvent frame_event : frame_events) {
    StatisticsEventType type = frame_event.type;
    StatisticsEventMediaType media_type = frame_event.media_type;
    FrameStatsMap* frame_stats_map = GetFrameStatsMapForMediaType(media_type);
    if (frame_stats_map) {
      auto it = frame_stats_map->find(type);
      if (it == frame_stats_map->end()) {
        FrameStatsAggregate stats;
        stats.event_counter = 1;
        stats.sum_size = frame_event.size;
        stats.sum_delay = frame_event.delay_delta;
        frame_stats_map->insert(std::make_pair(type, stats));
      } else {
        ++(it->second.event_counter);
        it->second.sum_size += frame_event.size;
        it->second.sum_delay += frame_event.delay_delta;
      }
    }

    RecordEventTimes(frame_event.timestamp, media_type,
                     IsReceiverEvent(frame_event.type));

    RecordFrameLatencies(frame_event);
  }
}

void StatisticsAnalyzer::ProcessPacketEvents(
    std::vector<PacketEvent> packet_events) {
  for (PacketEvent packet_event : packet_events) {
    StatisticsEventType type = packet_event.type;
    StatisticsEventMediaType media_type = packet_event.media_type;
    PacketStatsMap* packet_stats_map =
        GetPacketStatsMapForMediaType(media_type);
    if (packet_stats_map) {
      auto it = packet_stats_map->find(type);
      if (it == packet_stats_map->end()) {
        PacketStatsAggregate stats;
        stats.event_counter = 1;
        stats.sum_size = packet_event.size;
        packet_stats_map->insert(std::make_pair(type, stats));
      } else {
        ++(it->second.event_counter);
        it->second.sum_size += packet_event.size;
      }
    }

    RecordEventTimes(packet_event.timestamp, media_type,
                     IsReceiverEvent(packet_event.type));

    if (type == StatisticsEventType::kPacketSentToNetwork ||
        type == StatisticsEventType::kPacketReceived) {
      RecordPacketLatencies(packet_event);
    } else if (type == StatisticsEventType::kPacketRetransmitted) {
      // We only measure network latency for packets that are not retransmitted.
      ErasePacketInfo(packet_event);
    }
  }
}

void StatisticsAnalyzer::RecordFrameLatencies(FrameEvent frame_event) {
  FrameKey key =
      std::make_pair(frame_event.rtp_timestamp, frame_event.media_type);
  auto it = recent_frame_infos_.find(key);
  if (it == recent_frame_infos_.end()) {
    FrameInfo info;
    if (frame_event.type == StatisticsEventType::kFrameEncoded) {
      info.encode_end_time = frame_event.timestamp;
    }
    recent_frame_infos_.insert(std::make_pair(key, info));
    if (recent_frame_infos_.size() >= kMaxRecentFrameInfoMapSize) {
      recent_frame_infos_.erase(recent_frame_infos_.begin());
    }
  } else {
    FrameInfo value = it->second;

    // Frame latency is the time from when the frame is encoded until the
    // receiver ack for the frame is sent.
    if (frame_event.type == StatisticsEventType::kFrameAckSent) {
      // TODO(bzielinski): account for receiver offset
      Clock::duration frame_latency =
          frame_event.timestamp - value.encode_end_time;
      AddToLatencyAggregrate(StatisticType::kAvgFrameLatencyMs, frame_latency,
                             frame_event.media_type);
    } else if (frame_event.type == StatisticsEventType::kFramePlayedOut) {
      // Positive delay means the frame is late.
      if (frame_event.delay_delta > Clock::duration::zero()) {
        SessionStats* session_stats =
            GetSessionStatsForMediaType(frame_event.media_type);
        if (session_stats) {
          ++(session_stats->late_frame_counter);
        }
        AddToHistogram(HistogramType::kFrameLatenessMs, frame_event.media_type,
                       InMilliseconds(frame_event.delay_delta));
      }
    }
  }
}

void StatisticsAnalyzer::RecordPacketLatencies(PacketEvent packet_event) {
  // Queueing latency is the time from when a frame is encoded to when the
  // packet is first sent.
  if (packet_event.type == StatisticsEventType::kPacketSentToNetwork) {
    auto it = recent_frame_infos_.find(
        std::make_pair(packet_event.rtp_timestamp, packet_event.media_type));
    // We have an encode end time for a frame associated with this packet.
    if (it != recent_frame_infos_.end()) {
      Clock::duration queueing_latency =
          packet_event.timestamp - it->second.encode_end_time;
      AddToLatencyAggregrate(StatisticType::kAvgQueueingLatencyMs,
                             queueing_latency, packet_event.media_type);
      AddToHistogram(HistogramType::kQueueingLatencyMs, packet_event.media_type,
                     InMilliseconds(queueing_latency));
    }
  }

  StatisticsAnalyzer::PacketKey key =
      std::make_pair(packet_event.rtp_timestamp, packet_event.packet_id);
  auto it = recent_packet_infos_.find(key);
  if (it == recent_packet_infos_.end()) {
    PacketInfo info;
    info.timestamp = packet_event.timestamp;
    info.type = packet_event.type;
    recent_packet_infos_.insert(std::make_pair(key, info));
    if (recent_packet_infos_.size() > kMaxRecentPacketInfoMapSize) {
      recent_packet_infos_.erase(recent_packet_infos_.begin());
    }
  } else {  // We know when this packet was sent, and when it arrived.
    PacketInfo value = it->second;
    StatisticsEventType recorded_type = value.type;
    bool match = false;
    Clock::time_point packet_sent_time;
    Clock::time_point packet_received_time;
    if (recorded_type == StatisticsEventType::kPacketSentToNetwork &&
        packet_event.type == StatisticsEventType::kPacketReceived) {
      packet_sent_time = value.timestamp;
      packet_received_time = packet_event.timestamp;
      match = true;
    } else if (recorded_type == StatisticsEventType::kPacketReceived &&
               packet_event.type == StatisticsEventType::kPacketSentToNetwork) {
      packet_sent_time = packet_event.timestamp;
      packet_received_time = value.timestamp;
      match = true;
    }
    if (match) {
      recent_packet_infos_.erase(it);

      // TODO(bzielinski):Subtract by offset.
      // packet_received_time -= receiver_offset;

      // Network latency is the time between when a packet is sent and when it
      // is received.
      Clock::duration network_latency = packet_received_time - packet_sent_time;
      AddToLatencyAggregrate(StatisticType::kAvgNetworkLatencyMs,
                             network_latency, packet_event.media_type);
      AddToHistogram(HistogramType::kNetworkLatencyMs, packet_event.media_type,
                     InMilliseconds(network_latency));

      // Packet latency is the time from when a frame is encoded until when the
      // packet is received.
      auto frame_it = recent_frame_infos_.find(
          std::make_pair(packet_event.rtp_timestamp, packet_event.media_type));
      if (frame_it != recent_frame_infos_.end()) {
        Clock::duration packet_latency =
            packet_received_time - frame_it->second.encode_end_time;
        AddToLatencyAggregrate(StatisticType::kAvgPacketLatencyMs,
                               packet_latency, packet_event.media_type);
        AddToHistogram(HistogramType::kPacketLatencyMs, packet_event.media_type,
                       InMilliseconds(packet_latency));
      }
    }
  }
}

void StatisticsAnalyzer::RecordEventTimes(Clock::time_point timestamp,
                                          StatisticsEventMediaType media_type,
                                          bool is_receiver_event) {
  SessionStats* session_stats = GetSessionStatsForMediaType(media_type);
  if (!session_stats) {
    return;
  }

  if (is_receiver_event) {
    // TODO(bzielinski): consider offset.
    if (session_stats->last_response_received_time ==
        Clock::time_point::min()) {
      session_stats->last_response_received_time = timestamp;
    } else {
      session_stats->last_response_received_time =
          std::max(session_stats->last_response_received_time, timestamp);
    }
  }

  if (session_stats->first_event_time == Clock::time_point::min()) {
    session_stats->first_event_time = timestamp;
  } else {
    session_stats->first_event_time =
        std::min(session_stats->first_event_time, timestamp);
  }
  if (session_stats->last_event_time == Clock::time_point::min()) {
    session_stats->last_event_time = timestamp;
  } else {
    session_stats->last_event_time =
        std::max(session_stats->last_event_time, timestamp);
  }
}

void StatisticsAnalyzer::ErasePacketInfo(PacketEvent packet_event) {
  StatisticsAnalyzer::PacketKey key =
      std::make_pair(packet_event.rtp_timestamp, packet_event.packet_id);
  recent_packet_infos_.erase(key);
}

void StatisticsAnalyzer::AddToLatencyAggregrate(
    StatisticType latency_stat,
    Clock::duration latency_delta,
    StatisticsEventMediaType media_type) {
  LatencyStatsMap* latency_stats = GetLatencyStatsMapForMediaType(media_type);
  if (!latency_stats) {
    return;
  }

  auto it = latency_stats->find(latency_stat);
  if (it == latency_stats->end()) {
    LatencyStatsAggregate stats;
    stats.data_point_counter = 1;
    stats.sum_latency = latency_delta;
    latency_stats->insert(std::make_pair(latency_stat, stats));
  } else {
    ++(it->second.data_point_counter);
    it->second.sum_latency += latency_delta;
  }
}

void StatisticsAnalyzer::AddToHistogram(HistogramType histogram,
                                        StatisticsEventMediaType media_type,
                                        int64_t sample) {
  if (media_type == StatisticsEventMediaType::kAudio) {
    audio_histograms_[static_cast<int>(histogram)].Add(sample);
  } else if (media_type == StatisticsEventMediaType::kVideo) {
    video_histograms_[static_cast<int>(histogram)].Add(sample);
  }
}

StatisticsAnalyzer::FrameStatsMap*
StatisticsAnalyzer::GetFrameStatsMapForMediaType(
    StatisticsEventMediaType media_type) {
  switch (media_type) {
    case StatisticsEventMediaType::kAudio:
      return &audio_frame_stats_;
    case StatisticsEventMediaType::kVideo:
      return &video_frame_stats_;
    default:
      return nullptr;
  }
}

StatisticsAnalyzer::PacketStatsMap*
StatisticsAnalyzer::GetPacketStatsMapForMediaType(
    StatisticsEventMediaType media_type) {
  switch (media_type) {
    case StatisticsEventMediaType::kAudio:
      return &audio_packet_stats_;
    case StatisticsEventMediaType::kVideo:
      return &video_packet_stats_;
    default:
      return nullptr;
  }
}

StatisticsAnalyzer::LatencyStatsMap*
StatisticsAnalyzer::GetLatencyStatsMapForMediaType(
    StatisticsEventMediaType media_type) {
  switch (media_type) {
    case StatisticsEventMediaType::kAudio:
      return &audio_latency_stats_;
    case StatisticsEventMediaType::kVideo:
      return &video_latency_stats_;
    default:
      return nullptr;
  }
}

StatisticsAnalyzer::SessionStats*
StatisticsAnalyzer::GetSessionStatsForMediaType(
    StatisticsEventMediaType media_type) {
  switch (media_type) {
    case StatisticsEventMediaType::kAudio:
      return &audio_session_stats_;
    case StatisticsEventMediaType::kVideo:
      return &video_session_stats_;
    default:
      return nullptr;
  }
}

SenderStats::HistogramsList StatisticsAnalyzer::GetAudioHistograms() {
  SenderStats::HistogramsList histos_list;
  for (size_t i = 0; i < audio_histograms_.size(); i++) {
    histos_list[i] = audio_histograms_[i].Copy();
  }
  return histos_list;
}

SenderStats::HistogramsList StatisticsAnalyzer::GetVideoHistograms() {
  SenderStats::HistogramsList histos_list;
  for (size_t i = 0; i < video_histograms_.size(); i++) {
    histos_list[i] = video_histograms_[i].Copy();
  }
  return histos_list;
}

SenderStats::StatisticsList StatisticsAnalyzer::ConstructStatisticsList(
    Clock::time_point end_time,
    StatisticsEventMediaType media_type) {
  SenderStats::StatisticsList stats_list;
  // kEnqueueFps
  stats_list = PopulateFpsStat(StatisticsEventType::kFrameEncoded,
                               StatisticType::kEnqueueFps, stats_list,
                               media_type, end_time);
  // kAvgCaptureLatencyMs
  // Requires frame events for capture time start / end.

  // kAvgEncodeTimeMs
  // Requires frame events for encode start time.

  // kAvgQueueingLatencyMs
  stats_list = PopulateAvgLatencyStat(StatisticType::kAvgQueueingLatencyMs,
                                      stats_list, media_type);
  // kAvgNetworkLatencyMs
  stats_list = PopulateAvgLatencyStat(StatisticType::kAvgNetworkLatencyMs,
                                      stats_list, media_type);
  // kAvgPacketLatencyMs
  stats_list = PopulateAvgLatencyStat(StatisticType::kAvgPacketLatencyMs,
                                      stats_list, media_type);
  // kAvgFrameLatencyMs
  stats_list = PopulateAvgLatencyStat(StatisticType::kAvgFrameLatencyMs,
                                      stats_list, media_type);
  // kAvgEndToEndLatencyMs
  // Requires knowing the capture start time.

  // kEncodeRateKbps
  stats_list = PopulateFrameBitrateStat(StatisticsEventType::kFrameEncoded,
                                        StatisticType::kEncodeRateKbps,
                                        stats_list, media_type, end_time);

  // kPacketTransmissionRateKbps
  stats_list =
      PopulatePacketBitrateStat(StatisticsEventType::kPacketSentToNetwork,
                                StatisticType::kPacketTransmissionRateKbps,
                                stats_list, media_type, end_time);

  // kNumFramesCaptured
  // Requires knowing the capture start time.

  // kNumFramesDroppedByEncoder
  // Requires knowing the capture end time.

  // kNumPacketsSent
  stats_list = PopulatePacketCountStat(
      StatisticsEventType::kPacketSentToNetwork, StatisticType::kNumPacketsSent,
      stats_list, media_type);

  // kNumPacketsReceived
  stats_list = PopulatePacketCountStat(StatisticsEventType::kPacketReceived,
                                       StatisticType::kNumPacketsReceived,
                                       stats_list, media_type);

  // kTimeSinceLastReceiverResponseMs
  // kFirstEventTimeMs
  // kLastEventTimeMs
  // kNumLateFrames
  stats_list = PopulateSessionStats(stats_list, media_type, end_time);

  return stats_list;
}

SenderStats::StatisticsList StatisticsAnalyzer::PopulateFrameCountStat(
    StatisticsEventType event,
    StatisticType stat,
    SenderStats::StatisticsList stats_list,
    StatisticsEventMediaType media_type) {
  FrameStatsMap* stats_map = GetFrameStatsMapForMediaType(media_type);
  if (!stats_map) {
    return stats_list;
  }

  auto it = stats_map->find(event);
  if (it != stats_map->end()) {
    int count = it->second.event_counter;
    stats_list[static_cast<int>(stat)] = count;
  }

  return stats_list;
}

SenderStats::StatisticsList StatisticsAnalyzer::PopulatePacketCountStat(
    StatisticsEventType event,
    StatisticType stat,
    SenderStats::StatisticsList stats_list,
    StatisticsEventMediaType media_type) {
  PacketStatsMap* stats_map = GetPacketStatsMapForMediaType(media_type);
  if (!stats_map) {
    return stats_list;
  }

  auto it = stats_map->find(event);
  if (it != stats_map->end()) {
    int count = it->second.event_counter;
    stats_list[static_cast<int>(stat)] = count;
  }

  return stats_list;
}

SenderStats::StatisticsList StatisticsAnalyzer::PopulateFpsStat(
    StatisticsEventType event,
    StatisticType stat,
    SenderStats::StatisticsList stats_list,
    StatisticsEventMediaType media_type,
    Clock::time_point end_time) {
  FrameStatsMap* stats_map = GetFrameStatsMapForMediaType(media_type);
  if (!stats_map) {
    return stats_list;
  }

  auto it = stats_map->find(event);
  if (it != stats_map->end()) {
    double fps = 0.0;
    Clock::duration duration = end_time - start_time_;
    int count = it->second.event_counter;
    fps = (count / InMilliseconds(duration)) * 1000;
    stats_list[static_cast<int>(stat)] = fps;
  }

  return stats_list;
}

SenderStats::StatisticsList StatisticsAnalyzer::PopulateAvgLatencyStat(
    StatisticType stat,
    SenderStats::StatisticsList stats_list,
    StatisticsEventMediaType media_type) {
  LatencyStatsMap* latency_map = GetLatencyStatsMapForMediaType(media_type);
  if (!latency_map) {
    return stats_list;
  }

  auto it = latency_map->find(stat);
  if (it != latency_map->end() && it->second.data_point_counter > 0) {
    double avg_latency =
        InMilliseconds(it->second.sum_latency) / it->second.data_point_counter;
    stats_list[static_cast<int>(stat)] = avg_latency;
  }

  return stats_list;
}

SenderStats::StatisticsList StatisticsAnalyzer::PopulateFrameBitrateStat(
    StatisticsEventType event,
    StatisticType stat,
    SenderStats::StatisticsList stats_list,
    StatisticsEventMediaType media_type,
    Clock::time_point end_time) {
  FrameStatsMap* stats_map = GetFrameStatsMapForMediaType(media_type);
  if (!stats_map) {
    return stats_list;
  }

  auto it = stats_map->find(event);
  if (it != stats_map->end()) {
    double kbps = 0.0;
    Clock::duration duration = end_time - start_time_;
    kbps = it->second.sum_size / InMilliseconds(duration) * 8;

    stats_list[static_cast<int>(stat)] = kbps;
  }

  return stats_list;
}

SenderStats::StatisticsList StatisticsAnalyzer::PopulatePacketBitrateStat(
    StatisticsEventType event,
    StatisticType stat,
    SenderStats::StatisticsList stats_list,
    StatisticsEventMediaType media_type,
    Clock::time_point end_time) {
  PacketStatsMap* stats_map = GetPacketStatsMapForMediaType(media_type);
  if (!stats_map) {
    return stats_list;
  }

  auto it = stats_map->find(event);
  if (it != stats_map->end()) {
    double kbps = 0.0;
    Clock::duration duration = end_time - start_time_;
    kbps = it->second.sum_size / InMilliseconds(duration) * 8;

    stats_list[static_cast<int>(stat)] = kbps;
  }

  return stats_list;
}

SenderStats::StatisticsList StatisticsAnalyzer::PopulateSessionStats(
    SenderStats::StatisticsList stats_list,
    StatisticsEventMediaType media_type,
    Clock::time_point end_time) {
  SessionStats* session_stats = GetSessionStatsForMediaType(media_type);
  if (!session_stats) {
    return stats_list;
  }

  if (session_stats->first_event_time != Clock::time_point::min()) {
    stats_list[static_cast<int>(StatisticType::kFirstEventTimeMs)] =
        InMilliseconds(session_stats->first_event_time.time_since_epoch());
  }

  if (session_stats->last_event_time != Clock::time_point::min()) {
    stats_list[static_cast<int>(StatisticType::kLastEventTimeMs)] =
        InMilliseconds(session_stats->last_event_time.time_since_epoch());
  }

  if (session_stats->last_response_received_time != Clock::time_point::min()) {
    stats_list[static_cast<int>(
        StatisticType::kTimeSinceLastReceiverResponseMs)] =
        InMilliseconds(end_time - session_stats->last_response_received_time);
  }

  stats_list[static_cast<int>(StatisticType::kNumLateFrames)] =
      session_stats->late_frame_counter;

  return stats_list;
}

}  // namespace cast
}  // namespace openscreen
