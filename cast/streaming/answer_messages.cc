// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/answer_messages.h"

#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "cast/streaming/message_util.h"
#include "platform/base/error.h"
#include "util/osp_logging.h"
namespace openscreen {
namespace cast {

namespace {

static constexpr char kMessageKeyType[] = "type";
static constexpr char kMessageTypeAnswer[] = "ANSWER";

// List of ANSWER message fields.
static constexpr char kAnswerMessageBody[] = "answer";
static constexpr char kResult[] = "result";
static constexpr char kResultOk[] = "ok";
static constexpr char kResultError[] = "error";
static constexpr char kErrorMessageBody[] = "error";
static constexpr char kErrorCode[] = "code";
static constexpr char kErrorDescription[] = "description";

// 32kbps is sender default for audio.
static constexpr int kDefaultAudioMinBitRate = 32 * 1000;

// 300kbps is sender default for video.
static constexpr int kDefaultVideoMinBitRate = 300 * 1000;

Json::Value AspectRatioConstraintToJson(AspectRatioConstraint aspect_ratio) {
  switch (aspect_ratio) {
    case AspectRatioConstraint::kVariable:
      return Json::Value("receiver");
    case AspectRatioConstraint::kFixed:
    default:
      return Json::Value("sender");
  }
}

ErrorOr<AspectRatioConstraint> AspectRatioConstraintFromString(
    absl::string_view aspect_ratio) {
  if (aspect_ratio == "receiver") {
    return AspectRatioConstraint::kVariable;
  } else if (aspect_ratio == "sender") {
    return AspectRatioConstraint::kFixed;
  }
  return Error::Code::kParameterInvalid;
}

template <typename T>
Json::Value PrimitiveVectorToJson(const std::vector<T>& vec) {
  Json::Value array(Json::ValueType::arrayValue);
  array.resize(vec.size());

  for (Json::Value::ArrayIndex i = 0; i < vec.size(); ++i) {
    array[i] = Json::Value(vec[i]);
  }

  return array;
}

}  // namespace

// static
ErrorOr<AspectRatio> AspectRatio::FromString(absl::string_view value) {
  std::vector<absl::string_view> fields = absl::StrSplit(value, ':');
  if (fields.size() != 2) {
    return Error::Code::kParameterInvalid;
  }

  AspectRatio ratio;
  if (!absl::SimpleAtoi(fields[0], &ratio.width)) {
    return Error::Code::kParameterInvalid;
  }
  if (!absl::SimpleAtoi(fields[1], &ratio.height)) {
    return Error::Code::kParameterInvalid;
  }
  return ratio;
}

// static
ErrorOr<AudioConstraints> AudioConstraints::Parse(const Json::Value& root) {
  auto max_sample_rate = ParseInt(root["maxSampleRate"]);
  if (!max_sample_rate) {
    return std::move(max_sample_rate.error());
  }

  auto max_channels = ParseInt(root["maxChannels"]);
  if (!max_channels) {
    return std::move(max_channels.error());
  }

  auto min_bit_rate = ParseInt(root["minBitRate"]);
  if (!min_bit_rate) {
    min_bit_rate = kDefaultAudioMinBitRate;
  }

  auto max_bit_rate = ParseInt(root["maxBitRate"]);
  if (!max_bit_rate) {
    return std::move(max_bit_rate.error());
  }

  auto max_delay_ms = ParseInt(root["maxDelay"]);
  if (!max_delay_ms) {
    return std::move(max_delay_ms.error());
  }

  if (max_sample_rate.value() <= 0 || max_channels.value() <= 0 ||
      min_bit_rate.value() <= 0 ||
      max_bit_rate.value() < min_bit_rate.value() ||
      max_delay_ms.value() <= 0) {
    return Error::Code::kParameterInvalid;
  }
  return AudioConstraints{max_sample_rate.value(), max_channels.value(),
                          min_bit_rate.value(), max_bit_rate.value(),
                          std::chrono::milliseconds{max_delay_ms.value()}};
}

ErrorOr<Json::Value> AudioConstraints::ToJson() const {
  if (max_sample_rate <= 0 || max_channels <= 0 || min_bit_rate <= 0 ||
      max_bit_rate < min_bit_rate) {
    return CreateParameterError("AudioConstraints");
  }

  Json::Value root;
  root["maxSampleRate"] = max_sample_rate;
  root["maxChannels"] = max_channels;
  root["minBitRate"] = min_bit_rate;
  root["maxBitRate"] = max_bit_rate;
  root["maxDelay"] = Json::Value::Int64(max_delay.count());
  return root;
}

ErrorOr<Dimensions> Dimensions::Parse(const Json::Value& root) {
  auto width = ParseInt(root["width"]);
  if (!width) {
    return std::move(width.error());
  }

  auto height = ParseInt(root["height"]);
  if (!height) {
    return std::move(height.error());
  }

  auto frame_rate = ParsePositiveSimpleFraction(root["frameRate"]);
  if (!frame_rate) {
    return std::move(frame_rate.error());
  }

  if (width.value() <= 0 || height <= 0 || !frame_rate.value().is_positive()) {
    return Error::Code::kParameterInvalid;
  }
  return Dimensions{width.value(), height.value(),
                    std::move(frame_rate.value())};
}

ErrorOr<Json::Value> Dimensions::ToJson() const {
  if (width <= 0 || height <= 0 || !frame_rate.is_defined() ||
      !frame_rate.is_positive()) {
    return CreateParameterError("Dimensions");
  }

  Json::Value root;
  root["width"] = width;
  root["height"] = height;
  root["frameRate"] = frame_rate.ToString();
  return root;
}

// static
ErrorOr<VideoConstraints> VideoConstraints::Parse(const Json::Value& root) {
  auto max_pixels_per_second = ParseDouble(root["maxPixelsPerSecond"]);
  if (!max_pixels_per_second) {
    return std::move(max_pixels_per_second.error());
  }

  absl::optional<Dimensions> min_dimensions;
  auto min_dimensions_value = root["minDimensions"];
  if (min_dimensions_value) {
    auto min_dimensions_or_error =
        Dimensions::Parse(std::move(min_dimensions_value));
    if (!min_dimensions_or_error) {
      return std::move(min_dimensions_or_error.error());
    }
    min_dimensions = std::move(min_dimensions_or_error.value());
  }

  auto max_dimensions = Dimensions::Parse(root["maxDimensions"]);
  if (!max_dimensions) {
    return std::move(max_dimensions.error());
  }

  auto min_bit_rate = ParseInt(root["minBitRate"]);
  if (!min_bit_rate) {
    min_bit_rate = kDefaultVideoMinBitRate;
  }

  auto max_bit_rate = ParseInt(root["maxBitRate"]);
  if (!max_bit_rate || max_bit_rate.value() < min_bit_rate.value()) {
    return std::move(max_bit_rate.error());
  }

  auto max_delay_ms = ParseInt(root["maxDelay"]);
  if (!max_delay_ms) {
    return std::move(max_delay_ms.error());
  }

  if (max_pixels_per_second.value() <= 0 || min_bit_rate.value() <= 0 ||
      max_bit_rate.value() < min_bit_rate.value() || max_delay_ms <= 0) {
    return Error::Code::kParameterInvalid;
  }
  return VideoConstraints{max_pixels_per_second.value(),
                          std::move(min_dimensions),
                          std::move(max_dimensions.value()),
                          min_bit_rate.value(),
                          max_bit_rate.value(),
                          std::chrono::milliseconds{max_delay_ms.value()}};
}

ErrorOr<Json::Value> VideoConstraints::ToJson() const {
  if (max_pixels_per_second <= 0 || min_bit_rate <= 0 ||
      max_bit_rate < min_bit_rate || max_delay.count() <= 0) {
    return CreateParameterError("VideoConstraints");
  }

  Json::Value min_dimensions_json;
  if (min_dimensions) {
    auto error_or_min_dim = min_dimensions->ToJson();
    if (error_or_min_dim.is_error()) {
      return std::move(error_or_min_dim.error());
    }
    min_dimensions_json = std::move(error_or_min_dim.value());
  }

  auto error_or_max_dim = max_dimensions.ToJson();
  if (error_or_max_dim.is_error()) {
    return std::move(error_or_max_dim.error());
  }

  Json::Value root;
  root["maxPixelsPerSecond"] = max_pixels_per_second;
  root["minDimensions"] = std::move(min_dimensions_json);
  root["maxDimensions"] = error_or_max_dim.value();
  root["minBitRate"] = min_bit_rate;
  root["maxBitRate"] = max_bit_rate;
  root["maxDelay"] = Json::Value::Int64(max_delay.count());
  return root;
}

// static
ErrorOr<Constraints> Constraints::Parse(const Json::Value& root) {
  auto audio = AudioConstraints::Parse(root["audio"]);
  if (!audio) {
    return std::move(audio.error());
  }

  auto video = VideoConstraints::Parse(root["video"]);
  if (!video) {
    return std::move(video.error());
  }

  return Constraints{std::move(audio.value()), std::move(video.value())};
}

ErrorOr<Json::Value> Constraints::ToJson() const {
  auto audio_or_error = audio.ToJson();
  if (audio_or_error.is_error()) {
    return std::move(audio_or_error.error());
  }

  auto video_or_error = video.ToJson();
  if (video_or_error.is_error()) {
    return std::move(video_or_error.error());
  }

  Json::Value root;
  root["audio"] = audio_or_error.value();
  root["video"] = video_or_error.value();
  return root;
}

// static
ErrorOr<DisplayDescription> DisplayDescription::Parse(const Json::Value& root) {
  DisplayDescription description;

  auto dimensions_value = root["dimensions"];
  if (dimensions_value) {
    auto dimensions_or_error = Dimensions::Parse(dimensions_value);
    if (!dimensions_or_error) {
      return std::move(dimensions_or_error.error());
    }
    description.dimensions = std::move(dimensions_or_error.value());
  }

  auto aspect_ratio_value = root["aspectRatio"];
  if (aspect_ratio_value) {
    auto aspect_ratio_or_error = ParseString(aspect_ratio_value);
    if (!aspect_ratio_or_error) {
      return std::move(aspect_ratio_or_error.error());
    }
    auto aspect_ratio = AspectRatio::FromString(aspect_ratio_or_error.value());
    if (!aspect_ratio) {
      return std::move(aspect_ratio.error());
    }
    description.aspect_ratio = std::move(aspect_ratio.value());
  }

  auto scaling_value = root["scaling"];
  if (scaling_value) {
    auto scaling_or_error = ParseString(scaling_value);
    if (!scaling_or_error) {
      return std::move(scaling_or_error.error());
    }
    auto ratio_constraint =
        AspectRatioConstraintFromString(scaling_or_error.value());
    if (!ratio_constraint) {
      return std::move(ratio_constraint.error());
    }
    description.aspect_ratio_constraint = std::move(ratio_constraint.value());
  }

  return description;
}

ErrorOr<Json::Value> DisplayDescription::ToJson() const {
  Json::Value root;
  if (aspect_ratio) {
    if (aspect_ratio->width < 1 || aspect_ratio->height < 1) {
      return CreateParameterError("DisplayDescription");
    }
    root["aspectRatio"] =
        absl::StrCat(aspect_ratio->width, ":", aspect_ratio->height);
  }

  if (dimensions) {
    auto dimensions_or_error = dimensions->ToJson();
    if (dimensions_or_error.is_error()) {
      return std::move(dimensions_or_error.error());
    }
    root["dimensions"] = dimensions_or_error.value();
  }

  if (aspect_ratio_constraint) {
    root["scaling"] =
        AspectRatioConstraintToJson(aspect_ratio_constraint.value());
  }

  return root;
}

ErrorOr<Answer> Answer::Parse(const Json::Value& root) {
  CastMode cast_mode = CastMode::Parse(root["castMode"].asString());
  auto udp_port = ParseInt(root["udpPort"]);
  if (!udp_port) {
    return std::move(udp_port.error());
  }
  auto send_indexes = ParseIntArray(root["sendIndexes"]);
  if (!send_indexes) {
    return std::move(send_indexes.error());
  }
  auto ssrcs = ParseUintArray(root["ssrcs"]);
  if (!ssrcs) {
    return std::move(ssrcs.error());
  }

  // Constraints and display descriptions are optional fields, and maybe null in
  // the valid case.
  absl::optional<Constraints> constraints;
  auto constraints_json = root["constraints"];
  if (constraints_json) {
    auto constraints_or_error = Constraints::Parse(constraints_json);
    if (!constraints_or_error) {
      return std::move(constraints_or_error.error());
    }
    constraints = std::move(constraints_or_error.value());
  }
  absl::optional<DisplayDescription> display;
  auto display_json = root["display"];
  if (display_json) {
    auto display_or_error = DisplayDescription::Parse(display_json);
    if (!display_or_error) {
      return std::move(display_or_error.error());
    }
    display = std::move(display_or_error.value());
  }

  auto receiver_rtcp_event_log = ParseIntArray(root["receiverRtcpEventLog"]);
  if (!receiver_rtcp_event_log) {
    receiver_rtcp_event_log = std::vector<int>{};
  }
  auto receiver_rtcp_dscp = ParseIntArray(root["receiverRtcpDscp"]);
  if (!receiver_rtcp_dscp) {
    receiver_rtcp_dscp = std::vector<int>{};
  }

  // NOTE: we are extra cautious here because ErrorOr<bool> can decay
  // to bool in some confusing ways, so we avoid bool operators for this case.
  const ErrorOr<bool> supports_wifi_status_reporting =
      ParseBool(root["receiverGetStatus"]);
  if (supports_wifi_status_reporting.is_error()) {
    return std::move(supports_wifi_status_reporting.error());
  }

  auto rtp_extensions = ParseStringArray(root["rtpExtensions"]);
  if (!rtp_extensions) {
    rtp_extensions = std::vector<std::string>{};
  }

  return Answer{cast_mode,
                udp_port.value(),
                std::move(send_indexes.value()),
                std::move(ssrcs.value()),
                std::move(constraints),
                std::move(display),
                std::move(receiver_rtcp_event_log.value()),
                std::move(receiver_rtcp_dscp.value()),
                supports_wifi_status_reporting.value(),
                std::move(rtp_extensions.value())};
}

ErrorOr<Json::Value> Answer::ToJson() const {
  if (udp_port <= 0 || udp_port > 65535) {
    return CreateParameterError("Answer - UDP Port number");
  }

  Json::Value root;
  if (constraints) {
    auto constraints_or_error = constraints.value().ToJson();
    if (constraints_or_error.is_error()) {
      return std::move(constraints_or_error.error());
    }
    root["constraints"] = std::move(constraints_or_error.value());
  }

  if (display) {
    auto display_or_error = display.value().ToJson();
    if (display_or_error.is_error()) {
      return std::move(display_or_error.error());
    }
    root["display"] = std::move(display_or_error.value());
  }

  root["castMode"] = cast_mode.ToString();
  root["udpPort"] = udp_port;
  root["receiverGetStatus"] = supports_wifi_status_reporting;
  root["sendIndexes"] = PrimitiveVectorToJson(send_indexes);
  root["ssrcs"] = PrimitiveVectorToJson(ssrcs);
  if (!receiver_rtcp_event_log.empty()) {
    root["receiverRtcpEventLog"] =
        PrimitiveVectorToJson(receiver_rtcp_event_log);
  }
  if (!receiver_rtcp_dscp.empty()) {
    root["receiverRtcpDscp"] = PrimitiveVectorToJson(receiver_rtcp_dscp);
  }
  if (!rtp_extensions.empty()) {
    root["rtpExtensions"] = PrimitiveVectorToJson(rtp_extensions);
  }
  return root;
}

Json::Value Answer::ToAnswerMessage() const {
  auto json_or_error = ToJson();
  if (json_or_error.is_error()) {
    return CreateInvalidAnswer(std::move(json_or_error.error()));
  }

  Json::Value message_root;
  message_root[kMessageKeyType] = kMessageTypeAnswer;
  message_root[kAnswerMessageBody] = std::move(json_or_error.value());
  message_root[kResult] = kResultOk;
  return message_root;
}

Json::Value CreateInvalidAnswer(Error error) {
  Json::Value message_root;
  message_root[kMessageKeyType] = kMessageTypeAnswer;
  message_root[kResult] = kResultError;
  message_root[kErrorMessageBody][kErrorCode] = static_cast<int>(error.code());
  message_root[kErrorMessageBody][kErrorDescription] = error.message();

  return message_root;
}

}  // namespace cast
}  // namespace openscreen
