// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_CAPTURE_RECOMMENDATIONS_H_
#define CAST_STREAMING_CAPTURE_RECOMMENDATIONS_H_

#include <memory>

namespace openscreen {
namespace cast {

struct Answer;

// This namespace contains classes and functions to be used by senders for
// determining what constraints are recommended for the capture device, based on
// the limits reported by the receiver.
//
// A general note about recommendations: they are NOT maximum operational
// limits, instead they are targeted to provide a delightful cast experience.
// For example, if a receiver is connected to a 1080P display but cannot provide
// 1080P at a stable FPS with a good experience, 1080P will not be recommended.
namespace capture_recommendations {

// Default maximum delay for both audio and video. Used if the sender fails
// to provide any constraints.
constexpr int kDefaultMaxDelayMs = 4000;

// Bit rate limits, used for both audio and video streams.
struct BitRateLimits {
  // Minimum bit rate, in bits per second.
  int min_bit_rate;

  // Maximum bit rate, in bits per second.
  int max_bit_rate;
};

// Audio capture recommendations. Maximum delay is determined by buffer
// constraints, and capture bit rate may vary between limits as appropriate.
struct Audio {
  // Represents the recommended bit rate range.
  BitRateLimits bit_rate_limits;

  // Represents the maximum audio delay, in milliseconds.
  int max_delay_ms = kDefaultMaxDelayMs;
};

struct Dimensions {
  int width;
  int height;
  double frame_rate;

  inline bool operator<(const Dimensions& other) {
    return effective_bit_rate() < other.effective_bit_rate();
  }
  // The effective bit rate is the predicted average bit rate based on the
  // properties of the Dimensions instance, and is currently just the product.
  constexpr int effective_bit_rate() const {
    return static_cast<int>(static_cast<double>(width * height) * frame_rate);
  }
};

// TODO: move to impl file?
constexpr Dimensions kDefaultMinDimensions{320, 240, 30};
// Currently mirroring only supports 1080P.
constexpr Dimensions kDefaultMaxDimensions{1920, 1080, 30};

// Our default limits are merely the product of the minimum and maximum
// dimensions, and are only used if the receiver fails to give better
// constraint information.
constexpr BitRateLimits kDefaultVideoBitRateLimits{
    kDefaultMinDimensions.effective_bit_rate(),
    kDefaultMaxDimensions.effective_bit_rate()};

// Video capture recommendations.
struct Video {
  // Represents the recommended bit rate range.
  BitRateLimits bit_rate_limits = kDefaultVideoBitRateLimits;

  // Represents the recommended minimum resolution.
  Dimensions minimum = kDefaultMinDimensions;

  // Represents the recommended maximum resolution.
  Dimensions maximum = kDefaultMaxDimensions;

  // Indicates whether the receiver can scale frames from a different aspect
  // ratio, or if it needs to be done by the sender. Default is true, as we
  // may not know the aspect ratio that the receiver supports.
  bool supports_scaling = true;

  // Represents the maximum video delay, in milliseconds.
  int max_delay_ms = kDefaultMaxDelayMs;
};

// Outputted recommendations for usage by capture devices. Note that we always
// return both audio and video (it is up to the sender to determine what
// streams actually get created). If the receiver doesn't give us any
// information for making recommendations, the defaults are used.
struct Recommendations {
  // Audio specific recommendations.
  Audio audio;

  // Video specific recommendations.
  Video video;
};

Recommendations Recommend(const Answer& answer);

}  // namespace capture_recommendations
}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_CAPTURE_RECOMMENDATIONS_H_
