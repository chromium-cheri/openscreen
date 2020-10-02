// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/session_config.h"

#include <algorithm>
#include <utility>

#include "cast/streaming/capture_recommendations.h"

namespace openscreen {
namespace cast {

namespace {

Error ParamError(const char* message) {
  return Error(Error::Code::kParameterInvalid, message);
}

bool IsZero(int i) {
  return i == 0;
}

}  // namespace

SessionConfig::SessionConfig(Ssrc sender_ssrc,
                             Ssrc receiver_ssrc,
                             int rtp_timebase,
                             int channels,
                             std::chrono::milliseconds target_playout_delay,
                             std::array<uint8_t, 16> aes_secret_key,
                             std::array<uint8_t, 16> aes_iv_mask)
    : sender_ssrc(sender_ssrc),
      receiver_ssrc(receiver_ssrc),
      rtp_timebase(rtp_timebase),
      channels(channels),
      target_playout_delay(target_playout_delay),
      aes_secret_key(std::move(aes_secret_key)),
      aes_iv_mask(std::move(aes_iv_mask)) {}

SessionConfig::SessionConfig(const SessionConfig& other) = default;
SessionConfig::SessionConfig(SessionConfig&& other) noexcept = default;
SessionConfig& SessionConfig::operator=(const SessionConfig& other) = default;
SessionConfig& SessionConfig::operator=(SessionConfig&& other) noexcept =
    default;
SessionConfig::~SessionConfig() = default;

Error SessionConfig::CheckValidity() const {
  if (rtp_timebase <
      std::min(capture_recommendations::kDefaultAudioMinSampleRate,
               kRtpVideoTimebase)) {
    return ParamError("RTP timebase too low for use.");
  }
  if (channels <= 0) {
    return ParamError("Channel count must be positive.");
  }
  if (target_playout_delay.count() <= 0) {
    return ParamError("Target playout delay must be positive.");
  }
  if (std::all_of(aes_secret_key.begin(), aes_secret_key.end(), IsZero)) {
    return ParamError("Must have a non-zero AES secret key.");
  }
  if (std::all_of(aes_iv_mask.begin(), aes_iv_mask.end(), IsZero)) {
    return ParamError("Must have a non-zero AES IV mask.");
  }
  return Error::None();
}

}  // namespace cast
}  // namespace openscreen
