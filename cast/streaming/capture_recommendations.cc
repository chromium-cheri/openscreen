// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/capture_recommendations.h"

#include <algorithm>
#include <utility>

#include "cast/streaming/answer_messages.h"

namespace openscreen {
namespace cast {
namespace capture_recommendations {
namespace {

void ApplyDisplay(const DisplayDescription& description,
                  Recommendations* recommendations) {
  if (description.aspect_ratio_constraint &&
      description.aspect_ratio_constraint.value() ==
          AspectRatioConstraint::kFixed) {
    recommendations->video.supports_scaling = false;
  }

  // We should never exceed the display's dimensions, since it will always
  // force scaling.
  if (description.dimensions) {
    const double frame_rate =
        static_cast<double>(description.dimensions->frame_rate);
    recommendations->video.maximum =
        Dimensions{description.dimensions->width,
                   description.dimensions->height, frame_rate};
    recommendations->video.bit_rate_limits.maximum =
        recommendations->video.maximum.effective_bit_rate();
    recommendations->video.minimum.set_minimum(recommendations->video.maximum);
  }

  // If the receiver gives us an aspect ratio that doesn't match the display
  // dimensions they give us, the behavior is undefined from the spec.
  // Here we prioritize the aspect ratio, and the receiver can scale the frame
  // as they wish.
  double aspect_ratio;
  if (description.aspect_ratio) {
    aspect_ratio = static_cast<double>(description.aspect_ratio->width) /
                   description.aspect_ratio->height;
    recommendations->video.maximum.width =
        recommendations->video.maximum.height * aspect_ratio;
  } else if (description.dimensions) {
    aspect_ratio = static_cast<double>(description.dimensions->width) /
                   description.dimensions->height;
  } else {
    return;
  }
  recommendations->video.minimum.width =
      recommendations->video.minimum.height * aspect_ratio;
}

Dimensions ToDimensions(const cast::Dimensions& dims) {
  return {dims.width, dims.height, static_cast<double>(dims.frame_rate)};
}

void ApplyConstraints(const Constraints& constraints,
                      Recommendations* recommendations) {
  // Audio has no fields in the display description, so we can safely
  // ignore the current recommendations when setting values here.
  recommendations->audio.max_delay = constraints.audio.max_delay;
  recommendations->audio.max_channels = constraints.audio.max_channels;
  recommendations->audio.max_sample_rate = constraints.audio.max_sample_rate;

  recommendations->audio.bit_rate_limits = BitRateLimits{
      std::max(constraints.audio.min_bit_rate, kDefaultAudioMinBitRate),
      std::max(constraints.audio.max_bit_rate, kDefaultAudioMinBitRate)};

  // With video, we take the intersection of values of the constraints and
  // the display description.
  recommendations->video.max_delay = constraints.video.max_delay;
  recommendations->video.max_pixels_per_second =
      constraints.video.max_pixels_per_second;
  recommendations->video.bit_rate_limits =
      BitRateLimits{std::max(constraints.video.min_bit_rate,
                             recommendations->video.bit_rate_limits.minimum),
                    std::min(constraints.video.max_bit_rate,
                             recommendations->video.bit_rate_limits.maximum)};
  Dimensions max = ToDimensions(constraints.video.max_dimensions);
  if (kDefaultMinDimensions < max && max < recommendations->video.maximum) {
    recommendations->video.maximum = std::move(max);
  }
  if (constraints.video.min_dimensions) {
    Dimensions min = ToDimensions(constraints.video.min_dimensions.value());
    if (kDefaultMinDimensions < min) {
      recommendations->video.minimum = std::move(min);
    }
  }
}

}  // namespace

Recommendations Recommend(const Answer& answer) {
  Recommendations recommendations;
  if (answer.display.has_value() && answer.display->IsValid()) {
    ApplyDisplay(answer.display.value(), &recommendations);
  }
  if (answer.constraints.has_value() && answer.constraints->IsValid()) {
    ApplyConstraints(answer.constraints.value(), &recommendations);
  }
  return recommendations;
}

}  // namespace capture_recommendations
}  // namespace cast
}  // namespace openscreen
