// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_SENDER_PUBLIC_FRAME_SENDER_CONFIG_H_
#define CAST_SENDER_PUBLIC_FRAME_SENDER_CONFIG_H_

#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "cast/common/public/frame_agent_config.h"
#include "cast/sender/public/video_codec_params.h"
#include "platform/api/time.h"
#include "streaming/cast/rtp_defines.h"

namespace cast {
namespace api {

// The configuration used by frame sending classes, i.e. AudioSender
// and VideoSender. The underlying frame sending class is an implementation
// detail, however it is why video specific parameters are given here as
// optional.
struct FrameSenderConfig : public FrameAgentConfig {
  // The total amount of time between a frame capture and its playback on
  // the receiver. This is given as a range, so we can select a value for
  // use by the receiver.
  std::pair<Clock::duration, Clock::duration> playout_limits;

  // The initial playout delay for animated content.
  Clock::duration animated_playout_delay;

  // The type/encoding of frame data.
  openscreen::cast_streaming::RtpPayloadType payload_type;

  // If true, use an external hardware encoder.
  bool use_external_encoder;

  // Bitrate limits and initial value. Note: for audio, only
  // bitrate_limits.second (max) is used.
  std::pair<int, int> bitrate_limits;
  int initial_bitrate;

  // Maximum frame rate.
  double max_frame_rate;

  // Coded specific parameters. The embedder can choose to pass custom
  // parameters to the encoder here.
  std::vector<std::pair<std::string, std::string>> custom_codec_params;
  absl::optional<VideoCodecParams> video_codec_params;
};

}  // namespace api
}  // namespace cast

#endif  // CAST_SENDER_PUBLIC_FRAME_SENDER_CONFIG_H_
