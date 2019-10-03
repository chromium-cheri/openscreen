// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_PUBLIC_FRAME_AGENT_CONFIG_H_
#define CAST_COMMON_PUBLIC_FRAME_AGENT_CONFIG_H_

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "cast/common/public/codec.h"
#include "streaming/cast/rtp_defines.h"

namespace cast {
namespace api {

struct FrameRate {
  explicit FrameRate(int frame_rate_in) : frame_rate(frame_rate_in) {
    OSP_DCHECK_GE(frame_rate, 0) << "frame rates must be greater than zero.";
  }

  const int frame_rate;
}

struct PlayoutDelay {
  explicit PlayoutDelay(std::chrono::microseconds playout_delay_in)
  : playout_delay(playout_delay_in) {
    OSP_DCHECK_GE(playout_delay, 0) << "Playout delays must be greater than zero.";
  }

  const std::chrono::microseconds playout_delay;
}

// The general, parent config type for Cast Streaming senders and receivers that
// deal with frames (audio, video). Several configuration values must be shared
// between the sender and receiver to ensure compatibility during the session.
struct FrameAgentConfig {
  // The sender and receiver's SSRC identifiers. Note: SSRC identifiers
  // are defined as unsigned 32 bit integers here:
  // https://tools.ietf.org/html/rfc5576#page-5
  uint32_t sender_ssrc;
  uint32_t receiver_ssrc;

  // The type/encoding of frame data.
  // TODO(jophba): change type after Yuri's refactor patches land.
  openscreen::cast_streaming::RtpPayloadType payload_type;

  // RTP timebase: The number of RTP units advanced per second. For audio,
  // this is the sampling rate. For video, this is 90 kHz by convention.
  int rtp_timebase = 90000;

  // Number of channels. Must be 1 for video, for audio typically 2.
  int channels = 1;

  // The name of the codec used by the sender.
  Codec codec = Codec::kUnknown;

  // The AES-128 crypto key and initialization vector.
  std::array<uint8_t, 16> aes_secret_key;
  std::array<uint8_t, 16> aes_iv_mask;
};

}  // namespace api
}  // namespace cast

#endif  // CAST_COMMON_PUBLIC_FRAME_AGENT_CONFIG_H_
