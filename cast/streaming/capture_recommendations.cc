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
                  Recommendations* recommendations) {}

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
