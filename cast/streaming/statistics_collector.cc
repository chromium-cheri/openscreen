// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/statistics_collector.h"

#include <stdint.h>

#include <limits>
#include <utility>

#include "util/big_endian.h"

namespace openscreen {
namespace cast {

StatisticsCollector::StatisticsCollector(ClockNowFunctionPtr now) : now_(now) {}
StatisticsCollector::~StatisticsCollector() = default;

void StatisticsCollector::CollectPacketSent(ByteView packet) {
  PacketEvent event;

  // Populate the new PacketEvent by parsing the wire-format |packet|.
  event.timestamp = now_();
  // TODO(jophba): what about retransmissions??
  event.type = StatisticsEventType::kPacketSentToNetwork;

  BigEndianReader reader(packet.data(), packet.size());
  bool success = reader.Skip(4);
  uint32_t truncated_rtp_timestamp;
  success &= reader.Read<uint32_t>(&truncated_rtp_timestamp);
  uint32_t ssrc;
  success &= reader.Read<uint32_t>(&ssrc);

  // TODO(jophba): how to get this info?
  // auto it = sessions_.find(ssrc);
  // The session should always have been registered in |sessions_|.
  // DCHECK(it != sessions_.end());
  // event.rtp_timestamp = it->second.last_logged_rtp_timestamp_ =
  //     it->second.last_logged_rtp_timestamp_.Expand(truncated_rtp_timestamp);
  // event.media_type = it->second.is_audio ? AUDIO_EVENT : VIDEO_EVENT;

  success &= reader.Skip(2);
  success &= reader.Read<uint16_t>(&event.packet_id);
  success &= reader.Read<uint16_t>(&event.max_packet_id);

  // Check that the cast is safe.
  static_assert(std::numeric_limits<uint32_t>::max() <=
                    std::numeric_limits<size_t>::max(),
                "invalid type cast assumption");
  OSP_DCHECK_LE(packet.size(),
                static_cast<size_t>(std::numeric_limits<uint32_t>::max()));
  event.size = static_cast<uint32_t>(packet.size());
  OSP_DCHECK(success);

  recent_packet_events_.push_back(std::move(event));
}

void StatisticsCollector::CollectPacketEvent(const PacketEvent& event) {}

void StatisticsCollector::CollectFrameEvent(const FrameEvent& event) {}

}  // namespace cast
}  // namespace openscreen
