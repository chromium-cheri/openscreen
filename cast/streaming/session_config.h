// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_SESSION_CONFIG_H_
#define CAST_STREAMING_SESSION_CONFIG_H_

#include <array>
#include <chrono>  // NOLINT
#include <cstdint>
#include <string>

#include "streaming/cast/ssrc.h"

namespace cast {
namespace streaming {

// The general, parent config type for Cast Streaming senders and receivers that
// deal with frames (audio, video). Several configuration values must be shared
// between the sender and receiver to ensure compatibility during the session.
// TODO(jophba): add config validation.
struct SessionConfig {
  SessionConfig(openscreen::cast_streaming::Ssrc sender_ssrc_in,
                openscreen::cast_streaming::Ssrc receiver_ssrc_in,
                int rtp_timebase_in,
                int channels_in,
                std::array<uint8_t, 16> aes_secret_key_in,
                std::array<uint8_t, 16> aes_iv_mask_in);
  ~SessionConfig() = default;

  // The sender and receiver's SSRC identifiers. Note: SSRC identifiers
  // are defined as unsigned 32 bit integers here:
  // https://tools.ietf.org/html/rfc5576#page-5
  openscreen::cast_streaming::Ssrc sender_ssrc = 0;
  openscreen::cast_streaming::Ssrc receiver_ssrc = 0;

  // RTP timebase: The number of RTP units advanced per second. For audio,
  // this is the sampling rate. For video, this is 90 kHz by convention.
  int rtp_timebase = 90000;

  // Number of channels. Must be 1 for video, for audio typically 2.
  int channels = 1;

  // The AES-128 crypto key and initialization vector.
  std::array<uint8_t, 16> aes_secret_key{};
  std::array<uint8_t, 16> aes_iv_mask{};
};

}  // namespace streaming
}  // namespace cast

#endif  // CAST_STREAMING_SESSION_CONFIG_H_
