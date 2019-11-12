// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_RECEIVER_CONFIG_H_
#define CAST_STREAMING_RECEIVER_CONFIG_H_

#include <chrono>  // NOLINT

#include "cast/streaming/session_config.h"

namespace cast {
namespace streaming {

// The configuration used by the Receiver. Most of the values are shared
// in the underlying SessionConfig, however some settings must be configured
// specifically for the receiver.
struct ReceiverConfig : public SessionConfig {
  ReceiverConfig(openscreen::cast_streaming::Ssrc sender_ssrc_in,
                 openscreen::cast_streaming::Ssrc receiver_ssrc_in,
                 int rtp_timebase_in,
                 int channels_in,
                 std::array<uint8_t, 16> aes_secret_key_in,
                 std::array<uint8_t, 16> aes_iv_mask_in,
                 std::chrono::milliseconds target_playout_delay_in);
  ~ReceiverConfig() = default;

  // The total amount of time between a frame capture and its playback on
  // the receiver.
  std::chrono::milliseconds target_playout_delay{};
};

}  // namespace streaming
}  // namespace cast

#endif  // CAST_STREAMING_RECEIVER_CONFIG_H_
