
// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_API_FRAME_AGENT_CONFIG_H_
#define CAST_API_FRAME_AGENT_CONFIG_H_

#include <string>
#include <vector>
#include <utility>

#include "absl/types/optional.h"
#include "platform/api/time.h"
#include "cast/api/video_codec_params.h"
#include "streaming/cast/rtp_defines.h"

namespace cast {
namespace api {

using openscreen::platform::Clock;

// The general, parent config type for agents (receiver, sender) that deal
// with frames (audio, video). Several configuration values must be shared
// between the sender and receiver to ensure compatibility during the session.
struct FrameAgentConfig {
  // The sender and receiver's SSRC identifiers.
  uint32_t sender_ssrc;
  uint32_t receiver_ssrc;

  // The total amount of time between content capture and its playback on the
  // receiver, including network transport delay.
  Clock::duration target_playout_delay;

  // The type/encoding of frame data.
  openscreen::cast_streaming::RtpPayloadType payload_type;

  // RTP timebase: The number of RTP units advanced per second. For audio,
  // this is the sampling rate. For video, this is 90 kHz by convention.
  int rtp_timebase;

  // Number of channels. Must be 1 for video, for audio typically 2.
  int channels;

  // The target frame rate, in frames per second.
  double target_frame_rate;

  // The name of the codec used by the sender.
  std::string codec_name;

  // The AES crypto key and initialization vector.
  std::string aes_key;
  std::string aes_iv_mask;
};

}  // namespace api
}  // namespace cast

#endif  // CAST_API_FRAME_AGENT_CONFIG_H_
