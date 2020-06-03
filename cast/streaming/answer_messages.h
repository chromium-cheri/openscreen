// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_ANSWER_MESSAGES_H_
#define CAST_STREAMING_ANSWER_MESSAGES_H_

#include <array>
#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cast/streaming/ssrc.h"
#include "json/value.h"
#include "platform/base/error.h"
#include "util/simple_fraction.h"

namespace openscreen {
namespace cast {

// All of the structs below are intended to behave like PODs, and don't have any
// "json::Serializable" or "Parseable" base classes. This helps cut down on
// boilerplate a lot (we get constructors and destructors for free,
// essentially). All of these structs implement serialization methods:
// (1) ParseAndValidate. Shall return a boolean indicating whether the out
//     parameter is in a valid state after checking bounds and restrictions.
// (2) ToJson. Should return a proper JSON object, or a relevant Error on
//     any failures.
// (3) IsValid. Used by both ParseAndValidate and ToJson to ensure that the
//     object is in a good state.

// A small helper class to avoid use of abseil in the header, and also avoid
// heap allocations and having to write lots of boilerplate (at least 5
// constructors and a destructor for every class that would otherwise have
// to use a unique pointer).
template <typename T>
class Optional {
 public:
  Optional() : is_value_(false) {}
  explicit Optional(T value) : is_value_(true), value_(std::move(value)) {}
  Optional(const Optional& other) = default;
  Optional(Optional&& other) = default;
  Optional& operator=(const Optional& other) = default;
  Optional& operator=(Optional&& other) = default;
  Optional& operator=(const T& value) {
    is_value_ = true;
    value_ = value;
    return *this;
  }
  Optional& operator=(T&& value) {
    is_value_ = true;
    value_ = std::move(value);
    return *this;
  }
  T* operator->() { return &value_; }
  const T* operator->() const { return &value_; }
  operator bool() const { return is_value(); }
  ~Optional() = default;

  bool is_value() const { return is_value_; }
  const T& value() const { return value_; }
  T& value() { return value_; }

 private:
  bool is_value_;
  T value_;
};

struct AudioConstraints {
  static bool ParseAndValidate(const Json::Value& value, AudioConstraints* out);
  ErrorOr<Json::Value> ToJson() const;
  bool IsValid() const;

  int max_sample_rate = 0;
  int max_channels = 0;
  // Technically optional, sender will assume 32kbps if omitted.
  int min_bit_rate = 0;
  int max_bit_rate = 0;
  std::chrono::milliseconds max_delay = {};
};

struct Dimensions {
  static bool ParseAndValidate(const Json::Value& value, Dimensions* out);
  ErrorOr<Json::Value> ToJson() const;
  bool IsValid() const;

  int width = 0;
  int height = 0;
  SimpleFraction frame_rate;
};

struct VideoConstraints {
  static bool ParseAndValidate(const Json::Value& value, VideoConstraints* out);
  ErrorOr<Json::Value> ToJson() const;
  bool IsValid() const;

  double max_pixels_per_second = {};
  Optional<Dimensions> min_dimensions = {};
  Dimensions max_dimensions = {};
  // Technically optional, sender will assume 300kbps if omitted.
  int min_bit_rate = 0;
  int max_bit_rate = 0;
  std::chrono::milliseconds max_delay = {};
};

struct Constraints {
  static bool ParseAndValidate(const Json::Value& value, Constraints* out);
  ErrorOr<Json::Value> ToJson() const;
  bool IsValid() const;

  AudioConstraints audio;
  VideoConstraints video;
};

// Decides whether the Sender scales and letterboxes content to 16:9, or if
// it may send video frames of any arbitrary size and the Receiver must
// handle the presentation details.
enum class AspectRatioConstraint : uint8_t { kVariable = 0, kFixed };

struct AspectRatio {
  static bool ParseAndValidate(const Json::Value& value, AspectRatio* out);
  bool IsValid() const;

  bool operator==(const AspectRatio& other) const {
    return width == other.width && height == other.height;
  }
  bool operator!=(const AspectRatio& other) const { return !(*this == other); }

  int width = 0;
  int height = 0;
};

struct DisplayDescription {
  static bool ParseAndValidate(const Json::Value& value,
                               DisplayDescription* out);
  ErrorOr<Json::Value> ToJson() const;
  bool IsValid() const;

  // May exceed, be the same, or less than those mentioned in the
  // video constraints.
  Optional<Dimensions> dimensions;
  Optional<AspectRatio> aspect_ratio = {};
  Optional<AspectRatioConstraint> aspect_ratio_constraint = {};
};

struct Answer {
  static bool ParseAndValidate(const Json::Value& value, Answer* out);
  ErrorOr<Json::Value> ToJson() const;
  bool IsValid() const;

  // TODO(jophba): move to ReceiverSession.
  // In constrast to ToJson, ToAnswerMessage performs a successful serialization
  // even if the answer object is malformed, by complying to the spec's
  // error answer message format in this case.
  Json::Value ToAnswerMessage() const;

  int udp_port = 0;
  std::vector<int> send_indexes;
  std::vector<Ssrc> ssrcs;

  // Constraints and display descriptions are optional fields, and maybe null in
  // the valid case.
  Optional<Constraints> constraints;
  Optional<DisplayDescription> display;
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
