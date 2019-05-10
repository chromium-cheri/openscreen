// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_MOCK_COMPOUND_RTCP_PARSER_CLIENT_H_
#define STREAMING_CAST_MOCK_COMPOUND_RTCP_PARSER_CLIENT_H_

#include "streaming/cast/compound_rtcp_parser.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"

namespace openscreen {
namespace cast_streaming {

class MockCompoundRtcpParserClient : public CompoundRtcpParser::Client {
 public:
  MOCK_METHOD1(OnReceiverReferenceTimeAdvanced,
               void(platform::Clock::time_point reference_time));
  MOCK_METHOD1(OnReceiverReport, void(const RtcpReportBlock& receiver_report));
  MOCK_METHOD0(OnReceiverLostPicture, void());
  MOCK_METHOD2(OnReceiverCheckpoint,
               void(FrameId frame_id, std::chrono::milliseconds playout_delay));
  MOCK_METHOD1(OnReceiverHasFrames, void(absl::Span<const FrameId> frame_ids));
  MOCK_METHOD1(OnReceiverIsMissingFrames,
               void(absl::Span<const FrameId> frame_ids));
  MOCK_METHOD1(
      OnReceiverIsMissingPackets,
      void(absl::Span<const std::pair<FrameId, FramePacketId>> packet_ids));
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_MOCK_COMPOUND_RTCP_PARSER_CLIENT_H_
