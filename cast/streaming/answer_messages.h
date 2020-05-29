// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_ANSWER_MESSAGES_H_
#define CAST_STREAMING_ANSWER_MESSAGES_H_

#include <array>
#include <chrono>  // NOLINT
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cast/streaming/offer_messages.h"
#include "cast/streaming/ssrc.h"
#include "json/value.h"
#include "platform/base/error.h"
#include "util/simple_fraction.h"

namespace openscreen {
namespace cast {

struct AudioConstraints {
  static ErrorOr<AudioConstraints> Parse(const Json::Value& root);
  ErrorOr<Json::Value> ToJson() const;

  int max_sample_rate = 0;
  int max_channels = 0;
  // Technically optional, sender will assume 32kbps if omitted.
  int min_bit_rate = 0;
  int max_bit_rate = 0;
  std::chrono::milliseconds max_delay = {};
};

struct Dimensions {
  static ErrorOr<Dimensions> Parse(const Json::Value& root);
  ErrorOr<Json::Value> ToJson() const;

  int width = 0;
  int height = 0;
  SimpleFraction frame_rate;
};

struct VideoConstraints {
  static ErrorOr<VideoConstraints> Parse(const Json::Value& root);
  ErrorOr<Json::Value> ToJson() const;

  double max_pixels_per_second = {};
  absl::optional<Dimensions> min_dimensions = {};
  Dimensions max_dimensions = {};
  // Technically optional, sender will assume 300kbps if omitted.
  int min_bit_rate = 0;
  int max_bit_rate = 0;
  std::chrono::milliseconds max_delay = {};
};

struct Constraints {
  static ErrorOr<Constraints> Parse(const Json::Value& root);
  ErrorOr<Json::Value> ToJson() const;

  AudioConstraints audio;
  VideoConstraints video;
};

// Decides whether the Sender scales and letterboxes content to 16:9, or if
// it may send video frames of any arbitrary size and the Receiver must
// handle the presentation details.
enum class AspectRatioConstraint : uint8_t { kVariable = 0, kFixed };

struct AspectRatio {
  static ErrorOr<AspectRatio> FromString(absl::string_view value);
  bool operator==(const AspectRatio& other) const {
    return width == other.width && height == other.height;
  }
  bool operator!=(const AspectRatio& other) const { return !(*this == other); }

  int width = 0;
  int height = 0;
};

struct DisplayDescription {
  static ErrorOr<DisplayDescription> Parse(const Json::Value& root);
  ErrorOr<Json::Value> ToJson() const;

  // May exceed, be the same, or less than those mentioned in the
  // video constraints.
  absl::optional<Dimensions> dimensions;
  absl::optional<AspectRatio> aspect_ratio = {};
  absl::optional<AspectRatioConstraint> aspect_ratio_constraint = {};
};

struct Answer {
  static ErrorOr<Answer> Parse(const Json::Value& root);

  // ToJson performs a standard serialization, returning an error if this
  // instance failed to serialize properly.
  ErrorOr<Json::Value> ToJson() const;

  // In constrast to ToJson, ToAnswerMessage performs a successful serialization
  // even if the answer object is malformed, by complying to the spec's
  // error answer message format in this case.
  Json::Value ToAnswerMessage() const;

  CastMode cast_mode = {};
  int udp_port = 0;
  std::vector<int> send_indexes;
  std::vector<Ssrc> ssrcs;

  // Constraints and display descriptions are optional fields, and maybe null in
  // the valid case.
  absl::optional<Constraints> constraints;
  absl::optional<DisplayDescription> display;
  std::vector<int> receiver_rtcp_event_log;
  std::vector<int> receiver_rtcp_dscp;
  bool supports_wifi_status_reporting = false;

  // RTP extensions should be empty, but not null.
  std::vector<std::string> rtp_extensions = {};
};

// Helper method that creates an invalid Answer response. Exposed publicly
// here as it is called in ToAnswerMessage(), but can also be called by
// the receiver session.
Json::Value CreateInvalidAnswer(Error error);

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_ANSWER_MESSAGES_H_
