// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/capture_recommendations.h"

#include "absl/types/optional.h"
#include "cast/streaming/answer_messages.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace cast {
namespace capture_recommendations {
namespace {

constexpr Recommendations kDefaultRecommendations{
    Audio{BitRateLimits{320000, 256000}, 4000},
    Video{BitRateLimits{300000, 1920 * 1080 * 30}, Dimensions{320, 240, 30},
          Dimensions{1920, 1080, 30}, true, 4000}};

constexpr Constraints kEmptyConstraints{};

constexpr DisplayDescription kEmptyDescription{};

constexpr DisplayDescription kValidOnlyDimensions{
    cast::Dimensions{1024, 768, SimpleFraction{60, 1}}, absl::nullopt,
    absl::nullopt};

constexpr DisplayDescription kValidOnlyAspectRatio{
    absl::nullopt, AspectRatio{4, 3}, absl::nullopt};

constexpr DisplayDescription kValidOnlyAspectRatioSixteenNine{
    absl::nullopt, AspectRatio{16, 9}, absl::nullopt};

constexpr DisplayDescription kValidOnlyVariable{
    absl::nullopt, absl::nullopt, AspectRatioConstraint::kVariable};

constexpr DisplayDescription kInvalidOnlyFixed{absl::nullopt, absl::nullopt,
                                               AspectRatioConstraint::kFixed};

constexpr DisplayDescription kValidFixedAspectRatio{
    absl::nullopt, AspectRatio{4, 3}, AspectRatioConstraint::kFixed};

constexpr DisplayDescription kValidVariableAspectRatio{
    absl::nullopt, AspectRatio{4, 3}, AspectRatioConstraint::kVariable};

constexpr DisplayDescription kValidFixedMissingAspectRatio{
    cast::Dimensions{1024, 768, SimpleFraction{60, 1}}, absl::nullopt,
    AspectRatioConstraint::kFixed};

constexpr DisplayDescription kValidDisplayFhd{
    cast::Dimensions{1920, 1080, SimpleFraction{30, 1}}, AspectRatio{16, 9},
    AspectRatioConstraint::kVariable};

constexpr DisplayDescription kValidDisplayXga{
    cast::Dimensions{1024, 768, SimpleFraction{60, 1}}, AspectRatio{4, 3},
    AspectRatioConstraint::kFixed};

}  // namespace

TEST(CaptureRecommendationsTest, UsesDefaultsIfNoReceiverInformationAvailable) {
  Recommendations recommendations = Recommend(Answer{});
  EXPECT_EQ(kDefaultRecommendations, recommendations);
}

TEST(CaptureRecommendationsTest, EmptyConstraints) {
  Answer answer;
  answer.constraints = kEmptyConstraints;
  Recommendations recommendations = Recommend(answer);
  EXPECT_EQ(kDefaultRecommendations, recommendations);
}

TEST(CaptureRecommendationsTest, EmptyDisplayDescription) {
  Answer answer;
  answer.display = kEmptyDescription;
  Recommendations recommendations = Recommend(answer);
  EXPECT_EQ(kDefaultRecommendations, recommendations);
}

TEST(CaptureRecommendationsTest, OnlyDimensions) {
  Recommendations expected = kDefaultRecommendations;
  expected.video.maximum = Dimensions{1024, 768, 60.0};
  expected.video.bit_rate_limits.maximum = 47185920;
  Answer answer;
  answer.display = kValidOnlyDimensions;

  Recommendations recommendations = Recommend(answer);
  EXPECT_TRUE(expected == recommendations);
}

TEST(CaptureRecommendationsTest, OnlyAspectRatioFourThirds) {
  Recommendations expected = kDefaultRecommendations;
  expected.video.minimum = Dimensions{320, 240, 30.0};
  expected.video.maximum = Dimensions{1440, 1080, 30.0};
  expected.video.supports_scaling = true;
  Answer answer;
  answer.display = kValidOnlyAspectRatio;

  Recommendations recommendations = Recommend(answer);
  EXPECT_TRUE(expected == recommendations);
}

TEST(CaptureRecommendationsTest, OnlyAspectRatioSixteenNine) {
  Recommendations expected = kDefaultRecommendations;
  expected.video.minimum = Dimensions{426, 240, 30.0};
  expected.video.maximum = Dimensions{1920, 1080, 30.0};
  expected.video.supports_scaling = true;
  Answer answer;
  answer.display = kValidOnlyAspectRatioSixteenNine;

  Recommendations recommendations = Recommend(answer);
  EXPECT_TRUE(expected == recommendations);
}

TEST(CaptureRecommendationsTest, OnlyAspectRatioConstraint) {
  Answer answer;
  answer.display = kValidOnlyVariable;

  Recommendations recommendations = Recommend(answer);
  EXPECT_TRUE(kDefaultRecommendations == recommendations);
}

// It doesn't make sense to just provide a "fixed" aspect ratio with no
// other dimension information, so we just return default recommendations
// in this case and assume the sender will handle it elsewhere, e.g. on
// ANSWER message parsing.
TEST(CaptureRecommendationsTest, OnlyInvalidAspectRatioConstraint) {
  Recommendations expected = kDefaultRecommendations;
  expected.video.supports_scaling = false;
  Answer answer;
  answer.display = kInvalidOnlyFixed;

  Recommendations recommendations = Recommend(answer);
  EXPECT_TRUE(expected == recommendations);
}

TEST(CaptureRecommendationsTest, FixedAspectRatioConstraint) {
  Recommendations expected = kDefaultRecommendations;
  expected.video.minimum = Dimensions{320, 240, 30.0};
  expected.video.maximum = Dimensions{1440, 1080, 30.0};
  expected.video.supports_scaling = false;
  Answer answer;
  answer.display = kValidFixedAspectRatio;

  Recommendations recommendations = Recommend(answer);
  EXPECT_TRUE(expected == recommendations);
}

// Our behavior is actually the same whether the constraint is passed, we
// just percolate the constraint up to the capture devices so that intermediate
// frame sizes between minimum and maximum can be properly scaled.
TEST(CaptureRecommendationsTest, VariableAspectRatioConstraint) {
  Recommendations expected = kDefaultRecommendations;
  expected.video.minimum = Dimensions{320, 240, 30.0};
  expected.video.maximum = Dimensions{1440, 1080, 30.0};
  Answer answer;
  answer.display = kValidVariableAspectRatio;

  Recommendations recommendations = Recommend(answer);
  EXPECT_TRUE(expected == recommendations);
}

TEST(CaptureRecommendationsTest, DimensionsWithFixedConstraint) {
  Recommendations expected = kDefaultRecommendations;
  expected.video.minimum = Dimensions{320, 240, 30.0};
  expected.video.maximum = Dimensions{1024, 768, 60.0};
  expected.video.supports_scaling = false;
  expected.video.bit_rate_limits.maximum = 47185920;
  Answer answer;
  answer.display = kValidFixedMissingAspectRatio;

  Recommendations recommendations = Recommend(answer);
  EXPECT_TRUE(expected == recommendations);
}

TEST(CaptureRecommendationsTest, ExplicitFhdChangesMinimum) {
  Answer answer;
  answer.display = kValidDisplayFhd;
  Recommendations expected = kDefaultRecommendations;
  expected.video.minimum = Dimensions{426, 240, 30.0};

  Recommendations recommendations = Recommend(answer);
  EXPECT_EQ(expected, recommendations);
}

TEST(CaptureRecommendationsTest, XgaDimensions) {
  Recommendations expected = kDefaultRecommendations;
  expected.video.minimum = Dimensions{320, 240, 30.0};
  expected.video.maximum = Dimensions{1024, 768, 60.0};
  expected.video.supports_scaling = false;
  expected.video.bit_rate_limits.maximum = 47185920;
  Answer answer;
  answer.display = kValidDisplayXga;

  Recommendations recommendations = Recommend(answer);
  EXPECT_TRUE(expected == recommendations);
}

}  // namespace capture_recommendations
}  // namespace cast
}  // namespace openscreen
