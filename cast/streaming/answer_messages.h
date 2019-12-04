// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_ANSWER_MESSAGES_H_
#define CAST_STREAMING_ANSWER_MESSAGES_H_

#include <array>
#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

#include "cast/streaming/offer_messages.h"
#include "cast/streaming/ssrc.h"
#include "json/value.h"
#include "platform/base/error.h"

namespace cast {
namespace streaming {

struct AudioConstraints {
  int max_sample_rate = {};
  int max_channels = {};
  // Technically optional, sender will assume 32kbps if omitted.
  int min_bit_rate = {};
  int max_bit_rate = {};
  std::chrono::milliseconds max_delay = {};

  openscreen::ErrorOr<Json::Value> ToJson() const;
};

struct Dimensions {
  int width = {};
  int height = {};
  int frame_rate_numerator = {};
  int frame_rate_denominator = {};

  openscreen::ErrorOr<Json::Value> ToJson() const;
};

struct VideoConstraints {
  double max_pixels_per_second = {};
  Dimensions min_dimensions = {};
  Dimensions max_dimensions = {};
  // Technically optional, sender will assume 300kbps if omitted.
  int min_bit_rate = {};
  int max_bit_rate = {};
  std::chrono::milliseconds max_delay = {};

  openscreen::ErrorOr<Json::Value> ToJson() const;
};

struct Constraints {
  AudioConstraints audio = {};
  VideoConstraints video = {};

  openscreen::ErrorOr<Json::Value> ToJson() const;
};

// Decides whether the Sender scales and letterboxes content to 16:9, or if
// it may send video frames of any arbitrary size and the Receiver must
// handle the presentation details.
enum class AspectRatio : uint8_t { kAny = 0, kForce16x9 };

struct DisplayDescription {
  // May exceed, be the same, or less than those mentioned in constraints.
  Dimensions dimensions = {};
  std::pair<int, int> aspect_ratio = {};
  AspectRatio scaling = {};

  openscreen::ErrorOr<Json::Value> ToJson() const;
};

struct Answer {
  CastMode cast_mode = {};
  int udp_port = {};
  std::vector<int> send_indexes = {};
  std::vector<Ssrc> ssrcs = {};

  Constraints constraints = {};
  DisplayDescription display = {};
  std::vector<int> receiver_rtcp_event_log = {};
  std::vector<int> receiver_rtcp_dscp = {};
  bool supports_wifi_status_reporting = {};

  // RTP extensions should be empty, but not null.
  std::vector<std::string> rtp_extensions = {};

  // ToJson performs a standard serialization, returning an error if this
  // instance failed to serialize properly.
  openscreen::ErrorOr<Json::Value> ToJson() const;

  // In constrast to ToJson, ToMessageBody performs a successful serialization
  // even if the answer object is malformed, by complying to the spec's
  // error answer message format in this case.
  Json::Value ToMessageBody() const;
};

Json::Value CreateInvalidAnswer(openscreen::Error error);

}  // namespace streaming
}  // namespace cast

#endif  // CAST_STREAMING_ANSWER_MESSAGES_H_
