// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_OFFER_MESSAGES_H_
#define CAST_STREAMING_OFFER_MESSAGES_H_

#include <chrono>
#include <vector>

#include "absl/types/optional.h"
#include "cast/streaming/session_config.h"
#include "json/value.h"
#include "platform/base/error.h"

// This file contains the implementation of the Cast V2 Mirroring Control
// Protocol offer object definition.
namespace cast {
namespace streaming {

// If the target delay provided by the sender is not bounded by
// [kMinTargetDelay, kMaxTargetDelay], it will be set to kDefaultTargetDelay.
constexpr auto kDefaultTargetDelay = std::chrono::milliseconds(100);
constexpr auto kMinTargetDelay = std::chrono::milliseconds(0);
constexpr auto kMaxTargetDelay = std::chrono::milliseconds(2000);

// RTP profile must be provided, and its type must be cast.
enum RtpProfile { kCast };

struct Stream {
  enum class Type : int { kAudioSource, kVideoSource };

  int index;
  Type type;
  std::string codec_name;
  RtpProfile rtp_profile;
  int rtp_payload_type;
  Ssrc ssrc;
  std::chrono::milliseconds target_delay;
  std::array<uint8_t, 16> aes_key;
  std::array<uint8_t, 16> aes_iv_mask;
  bool receiver_rtcp_event_log;
  std::string receiver_rtcp_dscp;
  int time_base;
};

struct AudioStream {
  Stream stream;
  int bit_rate;
  int channels;
};

struct Resolution {
  int width;
  int height;
};

struct VideoStream {
  Stream stream;
  std::string max_frame_rate;
  int max_bit_rate;
  std::string protection;
  std::string profile;
  std::string level;
  std::vector<Resolution> resolutions;
  std::string error_recovery_mode;
};

struct Offer {
  enum class CastMode : int { kRemoting, kMirroring };

  static openscreen::ErrorOr<Offer> Parse(const Json::Value& root);

  CastMode cast_mode;
  std::vector<AudioStream> audio_streams;
  std::vector<VideoStream> video_streams;
};

}  // namespace streaming
}  // namespace cast

#endif  // CAST_STREAMING_OFFER_MESSAGES_H_
