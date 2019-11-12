// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_SENDER_CONFIG_H_
#define CAST_STREAMING_SENDER_CONFIG_H_

#include <chrono>  // NOLINT
#include <memory>
#include <utility>

#include "cast/streaming/session_config.h"
#include "cast/streaming/video_codec_params.h"
#include "streaming/cast/rtp_defines.h"

namespace cast {
namespace streaming {

// The configuration used by frame sending classes, i.e. AudioSender
// and VideoSender. The underlying frame sending class is an implementation
// detail, however it is why video specific parameters are given here as
// optional.
struct SenderConfig : public SessionConfig {
  // The type/encoding of frame data.
  // TODO(jophba): change type after Yuri's refactor patches land.
  openscreen::cast_streaming::RtpPayloadType payload_type =
      openscreen::cast_streaming::RtpPayloadType::kNull;

  // Bitrate limits and initial value. Note: for audio, only
  // bitrate_limits.second (max) is used.
  std::pair<int, int> bitrate_limits{};
  int initial_bitrate = 0;

  // Maximum frame rate.
  double max_frame_rate = 0;
};

}  // namespace streaming
}  // namespace cast

#endif  // CAST_STREAMING_SENDER_CONFIG_H_
