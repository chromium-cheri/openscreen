
// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_API_FRAME_RECEIVER_CONFIG_H_
#define CAST_API_FRAME_RECEIVER_CONFIG_H_

#include <string>
#include <vector>
#include <utility>

#include "absl/types/optional.h"
#include "platform/api/time.h"
#include "cast/api/video_codec_params.h"
#include "streaming/cast/rtp_defines.h"
#include "cast/api/frame_agent_config.h"

namespace cast {
namespace api {

// The configuration used by the FrameReceiver. Most of the values are shared
// in the underlying FrameAgentConfig, however some settings must be configured
// specifically for the receiver.
struct FrameReceiverConfig : public FrameAgentConfig {
  // The total amount of time between a frame capture and its playback on
  // the receiver.
  Clock::duration target_playout_delay;

  // The target frame rate, in frames per second.
  double target_frame_rate;
};

}  // namespace api
}  // namespace cast

#endif  // CAST_API_FRAME_RECEIVER_CONFIG_H_
