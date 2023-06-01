// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/statistics_defines.h"

namespace openscreen {
namespace cast {

const EnumNameTable<StatisticsEventType,
                    static_cast<size_t>(StatisticsEventType::kNumOfEvents)>
    kStatisticEventTypeNames = {
        {{"Unknown", StatisticsEventType::kUnknown},
         {"FrameCaptureBegin", StatisticsEventType::kFrameCaptureBegin},
         {"FrameCaptureEnd", StatisticsEventType::kFrameCaptureEnd},
         {"FrameEncoded", StatisticsEventType::kFrameEncoded},
         {"FrameAckSent", StatisticsEventType::kFrameAckSent},
         {"FrameDecoded", StatisticsEventType::kFrameDecoded},
         {"FramePlayedOut", StatisticsEventType::kFramePlayedOut},
         {"PacketSentToNetwork", StatisticsEventType::kPacketSentToNetwork},
         {"PacketRetransmitted", StatisticsEventType::kPacketRetransmitted},
         {"PacketRtxRejected", StatisticsEventType::kPacketRtxRejected},
         {"PacketReceived", StatisticsEventType::kPacketReceived}}};

FrameEvent::FrameEvent() = default;
FrameEvent::FrameEvent(const FrameEvent& other) = default;
FrameEvent::FrameEvent(FrameEvent&& other) = default;
FrameEvent& FrameEvent::operator=(const FrameEvent& other) = default;
FrameEvent& FrameEvent::operator=(FrameEvent&& other) = default;
FrameEvent::~FrameEvent() = default;

PacketEvent::PacketEvent() = default;
PacketEvent::PacketEvent(const PacketEvent& other) = default;
PacketEvent::PacketEvent(PacketEvent&& other) = default;
PacketEvent& PacketEvent::operator=(const PacketEvent& other) = default;
PacketEvent& PacketEvent::operator=(PacketEvent&& other) = default;
PacketEvent::~PacketEvent() = default;

}  // namespace cast
}  // namespace openscreen
