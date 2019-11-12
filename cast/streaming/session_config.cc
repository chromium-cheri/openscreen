// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/session_config.h"

namespace cast {
namespace streaming {

SessionConfig::SessionConfig(openscreen::cast_streaming::Ssrc sender_ssrc_in,
                             openscreen::cast_streaming::Ssrc receiver_ssrc_in,
                             int rtp_timebase_in,
                             int channels_in,
                             std::array<uint8_t, 16> aes_secret_key_in,
                             std::array<uint8_t, 16> aes_iv_mask_in)
    : sender_ssrc(sender_ssrc_in),
      receiver_ssrc(receiver_ssrc_in),
      rtp_timebase(rtp_timebase_in),
      channels(channels_in),
      aes_secret_key(aes_secret_key_in),
      aes_iv_mask(aes_iv_mask_in) {}

}  // namespace streaming
}  // namespace cast
