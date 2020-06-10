// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/capture_recommendations.h"

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

  // If we have a reasonable aspect ratio, we should apply it to our
  // minimum and maximum values as appropriate.
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

void ApplyConstraints(const Constraints& constraints,
                      Recommendations* recommendations) {}

}  // namespace

Recommendations Recommend(const Answer& answer) {
  Recommendations recommendations;
  if (answer.display.has_value()) {
    ApplyDisplay(answer.display.value(), &recommendations);
  }
  if (answer.constraints.has_value()) {
    ApplyConstraints(answer.constraints.value(), &recommendations);
  }
  return recommendations;
}

}  // namespace capture_recommendations
}  // namespace cast
}  // namespace openscreen
