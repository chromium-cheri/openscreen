// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_CAPTURE_RECOMMENDATIONS_H_
#define CAST_STREAMING_CAPTURE_RECOMMENDATIONS_H_

#include <cmath>
#include <memory>
#include <tuple>

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
  inline bool operator==(const BitRateLimits& other) const {
    return std::tie(minimum, maximum) == std::tie(other.minimum, other.maximum);
  }

  // Minimum bit rate, in bits per second.
  int minimum;

  // Maximum bit rate, in bits per second.
  int maximum;
};

// The mirroring control protocol specifies 32kbps as the absolute minimum
// for audio. Depending on the type of audio content (narrowband, fullband,
// etc.) Opus specifically can perform very well at this bitrate.
// See: https://research.google/pubs/pub41650/
constexpr int kDefaultAudioMinBitRate = 320 * 1000;

// Opus generally sees little improvement above 192kbps, but some older codecs
// that we may consider supporting improve at up to 256kbps.
constexpr int kDefaultAudioMaxBitRate = 256 * 1000;

constexpr BitRateLimits kDefaultAudioBitRateLimits{kDefaultAudioMinBitRate,
                                                   kDefaultAudioMaxBitRate};

// Audio capture recommendations. Maximum delay is determined by buffer
// constraints, and capture bit rate may vary between limits as appropriate.
struct Audio {
  inline bool operator==(const Audio& other) const {
    return std::tie(bit_rate_limits, max_delay_ms) ==
           std::tie(other.bit_rate_limits, other.max_delay_ms);
  }

  // Represents the recommended bit rate range.
  BitRateLimits bit_rate_limits = kDefaultAudioBitRateLimits;

  // Represents the maximum audio delay, in milliseconds.
  int max_delay_ms = kDefaultMaxDelayMs;
};

struct Dimensions {
  inline bool operator==(const Dimensions& other) const {
    const double kEpsilon = .0001;
    return (std::tie(width, height) == std::tie(other.width, other.height)) &&
           (std::abs(frame_rate - other.frame_rate) < kEpsilon);
  }

  int width;
  int height;
  double frame_rate;

  inline bool operator<(const Dimensions& other) const {
    return effective_bit_rate() < other.effective_bit_rate();
  }

  inline void set_minimum(const Dimensions& other) {
    if (other < *this) {
      *this = other;
    }
  }

  // The effective bit rate is the predicted average bit rate based on the
  // properties of the Dimensions instance, and is currently just the product.
  constexpr int effective_bit_rate() const {
    return static_cast<int>(static_cast<double>(width * height) * frame_rate);
  }
};

// The minimum dimensions are as close as possible to low-definition
// television, factoring in the receiver's aspect ratio if provided.
constexpr Dimensions kDefaultMinDimensions{320, 240, 30};

// Currently mirroring only supports 1080P.
constexpr Dimensions kDefaultMaxDimensions{1920, 1080, 30};

// The mirroring spec suggests 300kbps as the absolute minimum bitrate.
constexpr int kDefaultVideoMinBitRate = 300 * 1000;

// Our default limits are merely the product of the minimum and maximum
// dimensions, and are only used if the receiver fails to give better
// constraint information.
constexpr BitRateLimits kDefaultVideoBitRateLimits{
    kDefaultVideoMinBitRate, kDefaultMaxDimensions.effective_bit_rate()};

// Video capture recommendations.
struct Video {
  inline bool operator==(const Video& other) const {
    return std::tie(bit_rate_limits, minimum, maximum, supports_scaling,
                    max_delay_ms) ==
           std::tie(other.bit_rate_limits, other.minimum, other.maximum,
                    other.supports_scaling, other.max_delay_ms);
  }

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
  inline bool operator==(const Recommendations& other) const {
    return std::tie(audio, video) == std::tie(other.audio, other.video);
  }

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
