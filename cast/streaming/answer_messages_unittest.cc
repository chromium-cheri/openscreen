// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/answer_messages.h"

#include <chrono>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/chrono_helpers.h"
#include "util/json/json_serialization.h"

namespace openscreen {
namespace cast {

namespace {

using ::testing::ElementsAre;

// NOTE: the castMode property has been removed from the specification. We leave
// it here in the valid offer to ensure that its inclusion does not break
// parsing.
constexpr char kValidAnswerJson[] = R"({
  "castMode": "mirroring",
  "udpPort": 1234,
  "sendIndexes": [1, 3],
  "ssrcs": [1233324, 2234222],
  "constraints": {
    "audio": {
      "maxSampleRate": 96000,
      "maxChannels": 5,
      "minBitRate": 32000,
      "maxBitRate": 320000,
      "maxDelay": 5000
    },
    "video": {
      "maxPixelsPerSecond": 62208000,
      "minDimensions": {
        "width": 320,
        "height": 180,
        "frameRate": 0
      },
      "maxDimensions": {
        "width": 1920,
        "height": 1080,
        "frameRate": "60"
      },
      "minBitRate": 300000,
      "maxBitRate": 10000000,
      "maxDelay": 5000
    }
  },
  "display": {
    "dimensions": {
      "width": 1920,
      "height": 1080,
      "frameRate": "60000/1001"
    },
    "aspectRatio": "64:27",
    "scaling": "sender"
  },
  "receiverRtcpEventLog": [0, 1],
  "receiverRtcpDscp": [234, 567],
  "receiverGetStatus": true,
  "rtpExtensions": ["adaptive_playout_delay"]
})";

const Answer kValidAnswer{
    1234,                         // udp_port
    std::vector<int>{1, 2, 3},    // send_indexes
    std::vector<Ssrc>{123, 456},  // ssrcs
    Optional<Constraints>(Constraints{
        AudioConstraints{
            96000,              // max_sample_rate
            7,                  // max_channels
            32000,              // min_bit_rate
            96000,              // max_bit_rate
            milliseconds(2000)  // max_delay
        },                      // audio
        VideoConstraints{
            40000.0,  // max_pixels_per_second
            Optional<Dimensions>(Dimensions{
                320,                        // width
                480,                        // height
                SimpleFraction{15000, 101}  // frame_rate
            }),                             // min_dimensions
            Dimensions{
                1920,                   // width
                1080,                   // height
                SimpleFraction{288, 2}  // frame_rate
            },
            300000,             // min_bit_rate
            144000000,          // max_bit_rate
            milliseconds(3000)  // max_delay
        }                       // video
    }),                         // constraints
    Optional<DisplayDescription>(DisplayDescription{
        Optional<Dimensions>(Dimensions{
            640,                   // width
            480,                   // height
            SimpleFraction{30, 1}  // frame_rate
        }),
        Optional<AspectRatio>(AspectRatio{16, 9}),  // aspect_ratio
        Optional<AspectRatioConstraint>(
            AspectRatioConstraint::kFixed),  // scaling
    }),
    std::vector<int>{7, 8, 9},              // receiver_rtcp_event_log
    std::vector<int>{11, 12, 13},           // receiver_rtcp_dscp
    true,                                   // receiver_get_status
    std::vector<std::string>{"foo", "bar"}  // rtp_extensions
};

constexpr int kValidMaxPixelsPerSecond = 1920 * 1080 * 30;
constexpr Dimensions kValidDimensions{1920, 1080, SimpleFraction{60, 1}};
static const VideoConstraints kValidVideoConstraints{
    kValidMaxPixelsPerSecond, Optional<Dimensions>(kValidDimensions),
    kValidDimensions,         300 * 1000,
    300 * 1000 * 1000,        milliseconds(3000)};

constexpr AudioConstraints kValidAudioConstraints{123, 456, 300, 9920,
                                                  milliseconds(123)};

void ExpectEqualsValidAnswerJson(const Answer& answer) {
  EXPECT_EQ(1234, answer.udp_port);

  EXPECT_THAT(answer.send_indexes, ElementsAre(1, 3));
  EXPECT_THAT(answer.ssrcs, ElementsAre(1233324u, 2234222u));
  ASSERT_TRUE(answer.constraints);
  const AudioConstraints& audio = answer.constraints.value().audio;
  EXPECT_EQ(96000, audio.max_sample_rate);
  EXPECT_EQ(5, audio.max_channels);
  EXPECT_EQ(32000, audio.min_bit_rate);
  EXPECT_EQ(320000, audio.max_bit_rate);
  EXPECT_EQ(milliseconds{5000}, audio.max_delay);

  const VideoConstraints& video = answer.constraints.value().video;
  EXPECT_EQ(62208000, video.max_pixels_per_second);
  ASSERT_TRUE(video.min_dimensions);
  EXPECT_EQ(320, video.min_dimensions.value().width);
  EXPECT_EQ(180, video.min_dimensions.value().height);
  EXPECT_EQ((SimpleFraction{0, 1}), video.min_dimensions.value().frame_rate);
  EXPECT_EQ(1920, video.max_dimensions.width);
  EXPECT_EQ(1080, video.max_dimensions.height);
  EXPECT_EQ((SimpleFraction{60, 1}), video.max_dimensions.frame_rate);
  EXPECT_EQ(300000, video.min_bit_rate);
  EXPECT_EQ(10000000, video.max_bit_rate);
  EXPECT_EQ(milliseconds{5000}, video.max_delay);

  ASSERT_TRUE(answer.display);
  const DisplayDescription& display = answer.display.value();
  ASSERT_TRUE(display.dimensions);
  EXPECT_EQ(1920, display.dimensions.value().width);
  EXPECT_EQ(1080, display.dimensions.value().height);
  EXPECT_EQ((SimpleFraction{60000, 1001}),
            display.dimensions.value().frame_rate);
  EXPECT_EQ((AspectRatio{64, 27}), display.aspect_ratio.value());
  EXPECT_EQ(AspectRatioConstraint::kFixed,
            display.aspect_ratio_constraint.value());

  EXPECT_THAT(answer.receiver_rtcp_event_log, ElementsAre(0, 1));
  EXPECT_THAT(answer.receiver_rtcp_dscp, ElementsAre(234, 567));
  EXPECT_TRUE(answer.supports_wifi_status_reporting);
  EXPECT_THAT(answer.rtp_extensions, ElementsAre("adaptive_playout_delay"));
}

void ExpectFailureOnParse(absl::string_view raw_json) {
  ErrorOr<Json::Value> root = json::Parse(raw_json);
  // Must be a valid JSON object, but not a valid answer.
  ASSERT_TRUE(root.is_value());

  Answer answer;
  EXPECT_FALSE(Answer::ParseAndValidate(std::move(root.value()), &answer));
  EXPECT_FALSE(answer.IsValid());
}

// Functions that use ASSERT_* must return void, so we use an out parameter
// here instead of returning.
void ExpectSuccessOnParse(absl::string_view raw_json, Answer* out = nullptr) {
  ErrorOr<Json::Value> root = json::Parse(raw_json);
  // Must be a valid JSON object, but not a valid answer.
  ASSERT_TRUE(root.is_value());

  Answer answer;
  ASSERT_TRUE(Answer::ParseAndValidate(std::move(root.value()), &answer));
  EXPECT_TRUE(answer.IsValid());
  if (out) {
    *out = std::move(answer);
  }
}

}  // anonymous namespace

TEST(AnswerMessagesTest, ProperlyPopulatedAnswerSerializesProperly) {
  auto value_or_error = kValidAnswer.ToJson();
  EXPECT_TRUE(value_or_error.is_value());

  Json::Value root = std::move(value_or_error.value());
  EXPECT_EQ(root["udpPort"], 1234);

  Json::Value sendIndexes = std::move(root["sendIndexes"]);
  EXPECT_EQ(sendIndexes.type(), Json::ValueType::arrayValue);
  EXPECT_EQ(sendIndexes[0], 1);
  EXPECT_EQ(sendIndexes[1], 2);
  EXPECT_EQ(sendIndexes[2], 3);

  Json::Value ssrcs = std::move(root["ssrcs"]);
  EXPECT_EQ(ssrcs.type(), Json::ValueType::arrayValue);
  EXPECT_EQ(ssrcs[0], 123u);
  EXPECT_EQ(ssrcs[1], 456u);

  Json::Value constraints = std::move(root["constraints"]);
  Json::Value audio = std::move(constraints["audio"]);
  EXPECT_EQ(audio.type(), Json::ValueType::objectValue);
  EXPECT_EQ(audio["maxSampleRate"], 96000);
  EXPECT_EQ(audio["maxChannels"], 7);
  EXPECT_EQ(audio["minBitRate"], 32000);
  EXPECT_EQ(audio["maxBitRate"], 96000);
  EXPECT_EQ(audio["maxDelay"], 2000);

  Json::Value video = std::move(constraints["video"]);
  EXPECT_EQ(video.type(), Json::ValueType::objectValue);
  EXPECT_EQ(video["maxPixelsPerSecond"], 40000.0);
  EXPECT_EQ(video["minBitRate"], 300000);
  EXPECT_EQ(video["maxBitRate"], 144000000);
  EXPECT_EQ(video["maxDelay"], 3000);

  Json::Value min_dimensions = std::move(video["minDimensions"]);
  EXPECT_EQ(min_dimensions.type(), Json::ValueType::objectValue);
  EXPECT_EQ(min_dimensions["width"], 320);
  EXPECT_EQ(min_dimensions["height"], 480);
  EXPECT_EQ(min_dimensions["frameRate"], "15000/101");

  Json::Value max_dimensions = std::move(video["maxDimensions"]);
  EXPECT_EQ(max_dimensions.type(), Json::ValueType::objectValue);
  EXPECT_EQ(max_dimensions["width"], 1920);
  EXPECT_EQ(max_dimensions["height"], 1080);
  EXPECT_EQ(max_dimensions["frameRate"], "288/2");

  Json::Value display = std::move(root["display"]);
  EXPECT_EQ(display.type(), Json::ValueType::objectValue);
  EXPECT_EQ(display["aspectRatio"], "16:9");
  EXPECT_EQ(display["scaling"], "sender");

  Json::Value dimensions = std::move(display["dimensions"]);
  EXPECT_EQ(dimensions.type(), Json::ValueType::objectValue);
  EXPECT_EQ(dimensions["width"], 640);
  EXPECT_EQ(dimensions["height"], 480);
  EXPECT_EQ(dimensions["frameRate"], "30");

  Json::Value receiver_rtcp_event_log = std::move(root["receiverRtcpEventLog"]);
  EXPECT_EQ(receiver_rtcp_event_log.type(), Json::ValueType::arrayValue);
  EXPECT_EQ(receiver_rtcp_event_log[0], 7);
  EXPECT_EQ(receiver_rtcp_event_log[1], 8);
  EXPECT_EQ(receiver_rtcp_event_log[2], 9);

  Json::Value receiver_rtcp_dscp = std::move(root["receiverRtcpDscp"]);
  EXPECT_EQ(receiver_rtcp_dscp.type(), Json::ValueType::arrayValue);
  EXPECT_EQ(receiver_rtcp_dscp[0], 11);
  EXPECT_EQ(receiver_rtcp_dscp[1], 12);
  EXPECT_EQ(receiver_rtcp_dscp[2], 13);

  EXPECT_EQ(root["receiverGetStatus"], true);

  Json::Value rtp_extensions = std::move(root["rtpExtensions"]);
  EXPECT_EQ(rtp_extensions.type(), Json::ValueType::arrayValue);
  EXPECT_EQ(rtp_extensions[0], "foo");
  EXPECT_EQ(rtp_extensions[1], "bar");
}

TEST(AnswerMessagesTest, InvalidDimensionsCauseError) {
  Answer invalid_dimensions = kValidAnswer;
  invalid_dimensions.display.value().dimensions.value().width = -1;
  auto value_or_error = invalid_dimensions.ToJson();
  EXPECT_TRUE(value_or_error.is_error());
}

TEST(AnswerMessagesTest, InvalidAudioConstraintsCauseError) {
  Answer invalid_audio = kValidAnswer;
  invalid_audio.constraints.value().audio.max_bit_rate =
      invalid_audio.constraints.value().audio.min_bit_rate - 1;
  auto value_or_error = invalid_audio.ToJson();
  EXPECT_TRUE(value_or_error.is_error());
}

TEST(AnswerMessagesTest, InvalidVideoConstraintsCauseError) {
  Answer invalid_video = kValidAnswer;
  invalid_video.constraints.value().video.max_pixels_per_second = -1.0;
  auto value_or_error = invalid_video.ToJson();
  EXPECT_TRUE(value_or_error.is_error());
}

TEST(AnswerMessagesTest, InvalidDisplayDescriptionsCauseError) {
  Answer invalid_display = kValidAnswer;
  invalid_display.display.value().aspect_ratio = {0, 0};
  auto value_or_error = invalid_display.ToJson();
  EXPECT_TRUE(value_or_error.is_error());
}

TEST(AnswerMessagesTest, InvalidUdpPortsCauseError) {
  Answer invalid_port = kValidAnswer;
  invalid_port.udp_port = 65536;
  auto value_or_error = invalid_port.ToJson();
  EXPECT_TRUE(value_or_error.is_error());
}

TEST(AnswerMessagesTest, CanParseValidAnswerJson) {
  Answer answer;
  ExpectSuccessOnParse(kValidAnswerJson, &answer);
  ExpectEqualsValidAnswerJson(answer);
}

// In practice, the rtpExtensions, receiverRtcpDscp, and receiverRtcpEventLog
// fields may be missing from some receivers. We handle this case by treating
// them as empty.
TEST(AnswerMessagesTest, SucceedsWithMissingRtpFields) {
  ExpectSuccessOnParse(R"({
  "udpPort": 1234,
  "sendIndexes": [1, 3],
  "ssrcs": [1233324, 2234222],
  "receiverGetStatus": true
  })");
}

TEST(AnswerMessagesTest, ErrorOnEmptyAnswer) {
  ExpectFailureOnParse("{}");
}

TEST(AnswerMessagesTest, ErrorOnMissingUdpPort) {
  ExpectFailureOnParse(R"({
    "sendIndexes": [1, 3],
    "ssrcs": [1233324, 2234222],
    "receiverGetStatus": true
  })");
}

TEST(AnswerMessagesTest, ErrorOnMissingSsrcs) {
  ExpectFailureOnParse(R"({
    "udpPort": 1234,
    "sendIndexes": [1, 3],
    "receiverGetStatus": true
  })");
}

TEST(AnswerMessagesTest, ErrorOnMissingSendIndexes) {
  ExpectFailureOnParse(R"({
    "udpPort": 1234,
    "ssrcs": [1233324, 2234222],
    "receiverGetStatus": true
  })");
}

TEST(AnswerMessagesTest, AssumesNoReportingIfGetStatusFalse) {
  Answer answer;
  ExpectSuccessOnParse(R"({
    "udpPort": 1234,
    "sendIndexes": [1, 3],
    "ssrcs": [1233324, 2234222]
  })",
                       &answer);

  EXPECT_FALSE(answer.supports_wifi_status_reporting);
}

TEST(AnswerMessagesTest, AssumesMinBitRateIfOmitted) {
  Answer answer;
  ExpectSuccessOnParse(R"({
    "udpPort": 1234,
    "sendIndexes": [1, 3],
    "ssrcs": [1233324, 2234222],
    "constraints": {
      "audio": {
        "maxSampleRate": 96000,
        "maxChannels": 5,
        "maxBitRate": 320000,
        "maxDelay": 5000
      },
      "video": {
        "maxPixelsPerSecond": 62208000,
        "maxDimensions": {
          "width": 1920,
          "height": 1080,
          "frameRate": "60"
        },
        "maxBitRate": 10000000,
        "maxDelay": 5000
      }
    },
    "receiverGetStatus": true
  })",
                       &answer);

  EXPECT_EQ(32000, answer.constraints.value().audio.min_bit_rate);
  EXPECT_EQ(300000, answer.constraints.value().video.min_bit_rate);
}

// Instead of testing all possible json parsing options for validity, we
// can instead directly test the IsValid() methods.
TEST(AnswerMessagesTest, AudioConstraintsIsValid) {
  constexpr AudioConstraints kInvalidSampleRate{0, 456, 300, 9920,
                                                milliseconds(123)};
  constexpr AudioConstraints kInvalidMaxChannels{123, 0, 300, 9920,
                                                 milliseconds(123)};
  constexpr AudioConstraints kInvalidMinBitRate{123, 456, 0, 9920,
                                                milliseconds(123)};
  constexpr AudioConstraints kInvalidMaxBitRate{123, 456, 300, 0,
                                                milliseconds(123)};
  constexpr AudioConstraints kInvalidMaxDelay{123, 456, 300, 0,
                                              milliseconds(0)};

  EXPECT_TRUE(kValidAudioConstraints.IsValid());
  EXPECT_FALSE(kInvalidSampleRate.IsValid());
  EXPECT_FALSE(kInvalidMaxChannels.IsValid());
  EXPECT_FALSE(kInvalidMinBitRate.IsValid());
  EXPECT_FALSE(kInvalidMaxBitRate.IsValid());
  EXPECT_FALSE(kInvalidMaxDelay.IsValid());
}

TEST(AnswerMessagesTest, DimensionsIsValid) {
  // NOTE: in some cases (such as min dimensions) a frame rate of zero is valid.
  constexpr Dimensions kValidZeroFrameRate{1920, 1080, SimpleFraction{0, 60}};
  constexpr Dimensions kInvalidWidth{0, 1080, SimpleFraction{60, 1}};
  constexpr Dimensions kInvalidHeight{1920, 0, SimpleFraction{60, 1}};
  constexpr Dimensions kInvalidFrameRateZeroDenominator{1920, 1080,
                                                        SimpleFraction{60, 0}};
  constexpr Dimensions kInvalidFrameRateNegativeNumerator{
      1920, 1080, SimpleFraction{-1, 30}};
  constexpr Dimensions kInvalidFrameRateNegativeDenominator{
      1920, 1080, SimpleFraction{30, -1}};

  EXPECT_TRUE(kValidDimensions.IsValid());
  EXPECT_TRUE(kValidZeroFrameRate.IsValid());
  EXPECT_FALSE(kInvalidWidth.IsValid());
  EXPECT_FALSE(kInvalidHeight.IsValid());
  EXPECT_FALSE(kInvalidFrameRateZeroDenominator.IsValid());
  EXPECT_FALSE(kInvalidFrameRateNegativeNumerator.IsValid());
  EXPECT_FALSE(kInvalidFrameRateNegativeDenominator.IsValid());
}

TEST(AnswerMessagesTest, VideoConstraintsIsValid) {
  VideoConstraints invalid_max_pixels_per_second = kValidVideoConstraints;
  invalid_max_pixels_per_second.max_pixels_per_second = 0;

  VideoConstraints invalid_min_dimensions = kValidVideoConstraints;
  invalid_min_dimensions.min_dimensions->width = 0;

  VideoConstraints invalid_max_dimensions = kValidVideoConstraints;
  invalid_max_dimensions.max_dimensions.height = 0;

  VideoConstraints invalid_min_bit_rate = kValidVideoConstraints;
  invalid_min_bit_rate.min_bit_rate = 0;

  VideoConstraints invalid_max_bit_rate = kValidVideoConstraints;
  invalid_max_bit_rate.max_bit_rate = invalid_max_bit_rate.min_bit_rate - 1;

  VideoConstraints invalid_max_delay = kValidVideoConstraints;
  invalid_max_delay.max_delay = milliseconds(0);

  EXPECT_TRUE(kValidVideoConstraints.IsValid());
  EXPECT_FALSE(invalid_max_pixels_per_second.IsValid());
  EXPECT_FALSE(invalid_min_dimensions.IsValid());
  EXPECT_FALSE(invalid_max_dimensions.IsValid());
  EXPECT_FALSE(invalid_min_bit_rate.IsValid());
  EXPECT_FALSE(invalid_max_bit_rate.IsValid());
  EXPECT_FALSE(invalid_max_delay.IsValid());
}

TEST(AnswerMessagesTest, ConstraintsIsValid) {
  VideoConstraints invalid_video_constraints = kValidVideoConstraints;
  invalid_video_constraints.max_pixels_per_second = 0;

  AudioConstraints invalid_audio_constraints = kValidAudioConstraints;
  invalid_audio_constraints.max_bit_rate = 0;

  const Constraints valid{kValidAudioConstraints, kValidVideoConstraints};
  const Constraints invalid_audio{kValidAudioConstraints,
                                  invalid_video_constraints};
  const Constraints invalid_video{invalid_audio_constraints,
                                  kValidVideoConstraints};

  EXPECT_TRUE(valid.IsValid());
  EXPECT_FALSE(invalid_audio.IsValid());
  EXPECT_FALSE(invalid_video.IsValid());
}

TEST(AnswerMessagesTest, AspectRatioIsValid) {
  constexpr AspectRatio kValid{16, 9};
  constexpr AspectRatio kInvalidWidth{0, 9};
  constexpr AspectRatio kInvalidHeight{16, 0};

  EXPECT_TRUE(kValid.IsValid());
  EXPECT_FALSE(kInvalidWidth.IsValid());
  EXPECT_FALSE(kInvalidHeight.IsValid());
}

TEST(AnswerMessagesTest, DisplayDescriptionIsValid) {
  const DisplayDescription kInvalidEmptyDescription{
      Optional<Dimensions>{}, Optional<AspectRatio>{},
      Optional<AspectRatioConstraint>{}};

  DisplayDescription has_valid_dimensions = kInvalidEmptyDescription;
  has_valid_dimensions.dimensions = Optional<Dimensions>(kValidDimensions);

  DisplayDescription has_invalid_dimensions = kInvalidEmptyDescription;
  has_invalid_dimensions.dimensions = Optional<Dimensions>(kValidDimensions);
  has_invalid_dimensions.dimensions->width = 0;

  DisplayDescription has_aspect_ratio = kInvalidEmptyDescription;
  has_aspect_ratio.aspect_ratio = Optional<AspectRatio>{AspectRatio{16, 9}};

  DisplayDescription has_invalid_aspect_ratio = kInvalidEmptyDescription;
  has_invalid_aspect_ratio.aspect_ratio =
      Optional<AspectRatio>{AspectRatio{0, 20}};

  DisplayDescription has_aspect_ratio_constraint = kInvalidEmptyDescription;
  has_aspect_ratio_constraint.aspect_ratio_constraint =
      Optional<AspectRatioConstraint>(AspectRatioConstraint::kFixed);

  EXPECT_FALSE(kInvalidEmptyDescription.IsValid());
  EXPECT_TRUE(has_valid_dimensions.IsValid());
  EXPECT_FALSE(has_invalid_dimensions.IsValid());
  EXPECT_TRUE(has_aspect_ratio.IsValid());
  EXPECT_FALSE(has_invalid_aspect_ratio.IsValid());
  EXPECT_TRUE(has_aspect_ratio_constraint.IsValid());
}

// Instead of being tested here, Answer's IsValid is checked in all other
// relevant tests.

}  // namespace cast
}  // namespace openscreen
