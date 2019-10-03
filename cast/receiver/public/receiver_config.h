// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_RECEIVER_PUBLIC_RECEIVER_CONFIG_H_
#define CAST_RECEIVER_PUBLIC_RECEIVER_CONFIG_H_

#include <chrono>

#include "cast/common/public/frame_agent_config.h"

namespace cast {
namespace api {

// The configuration used by the Receiver. Most of the values are shared
// in the underlying AgentConfig, however some settings must be configured
// specifically for the receiver.
struct ReceiverConfig : public AgentConfig {
  // The total amount of time between a frame capture and its playback on
  // the receiver.
  PlayoutDelay target_playout_delay;

  // The target frame rate, in frames per second.
  FrameRate target_frame_rate;
};

}  // namespace api
}  // namespace cast

#endif  // CAST_RECEIVER_PUBLIC_RECEIVER_CONFIG_H_
