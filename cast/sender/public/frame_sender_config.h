// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_SENDER_PUBLIC_FRAME_SENDER_CONFIG_H_
#define CAST_SENDER_PUBLIC_FRAME_SENDER_CONFIG_H_

#include <chrono>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "cast/common/public/frame_agent_config.h"
#include "cast/sender/public/video_codec_params.h"
#include "streaming/cast/rtp_defines.h"

namespace cast {
namespace api {

struct Bitrate {
  explicit Bitrate(int bitrate_in) : bitrate(bitrate_in) {
    OSP_DCHECK(bitrate > 0) << "bitrate must be greater than zero";
  }

  const int bitrate;
};

// The configuration used by frame sending classes, i.e. AudioSender
// and VideoSender. The underlying frame sending class is an implementation
// detail, however it is why video specific parameters are given here as
// optional.
struct FrameSenderConfig : public FrameAgentConfig {
  // The total amount of time between a frame capture and its playback on
  // the receiver. This is given as a range, so we can select a value for
  // use by the receiver.
  std::pair<PlayoutDelay, PlayoutDelay> playout_limits;

  // The initial playout delay for animated content. Must be in the range
  // specified by playout_limits.
  PlayoutDelay animated_playout_delay;

  // The type/encoding of frame data.
  openscreen::cast_streaming::RtpPayloadType payload_type;

  // If true, use an external hardware encoder.
  bool use_external_encoder = false;

  // Bitrate limits and initial value. Note: for audio, only
  // bitrate_limits.second (max) is used.
  std::pair<Bitrate, Bitrate> bitrate_limits;
  Bitrate initial_bitrate;

  // Maximum frame rate.
  FrameRate max_frame_rate;

  // Optional video only codec parameters.
  absl::optional<VideoCodecParams> video_codec_params;
};

}  // namespace api
}  // namespace cast

#endif  // CAST_SENDER_PUBLIC_FRAME_SENDER_CONFIG_H_
