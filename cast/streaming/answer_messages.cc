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

/// NOTE: Constants here are all taken from the Cast V2: Mirroring Control
/// Protocol specification: http://go/mirroring-control-protocol
static constexpr char kMessageKeyType[] = "type";
static constexpr char kMessageTypeAnswer[] = "ANSWER";

/// ANSWER message fields.
static constexpr char kAnswerMessageBody[] = "answer";
static constexpr char kResult[] = "result";
static constexpr char kResultOk[] = "ok";
static constexpr char kResultError[] = "error";
static constexpr char kErrorMessageBody[] = "error";
static constexpr char kErrorCode[] = "code";
static constexpr char kErrorDescription[] = "description";

/// Constraint properties.
// Audio constraints. See properties below.
static constexpr char kAudio[] = "audio";
// Video constraints. See properties below.
static constexpr char kVideo[] = "video";

// An optional field representing the minimum bits per second. If not specified,
// the sender will use defaults stored below. This should represent the true
// operational minimum. See kDefaultAudioMinBitRate and kDefaultVideoMinBitRate
// below for the sender defaults.
static constexpr char kMinBitRate[] = "minBitRate";
// 32kbps is sender default for audio minimum bit rate.
static constexpr int kDefaultAudioMinBitRate = 32 * 1000;
// 300kbps is sender default for video minimum bit rate.
static constexpr int kDefaultVideoMinBitRate = 300 * 1000;

// Maximum encoded bits per second. This is the lower of (1) the max capability
// of the decoder, or (2) the max data transfer rate.
static constexpr char kMaxBitRate[] = "maxBitRate";
// Maximum supported end-to-end latency, in milliseconds. Proportional to the
// size of the data buffers in the receiver.
static constexpr char kMaxDelay[] = "maxDelay";

/// Video constraint properties.
// Maximum pixel rate (width * height * framerate). Is often less than
// multiplying the fields in maxDimensions. This field is used to set the
// maximum processing rate.
static constexpr char kMaxPixelsPerSecond[] = "maxPixelsPerSecond";
// Minimum dimensions. If omitted, the sender will assume a reasonable minimum
// with the same aspect ratio as maxDimensions, as close to 320*180 as possible.
// Should reflect the true operational minimum.
static constexpr char kMinDimensions[] = "minDimensions";
// Maximum dimensions, not necessarily ideal dimensions.
static constexpr char kMaxDimensions[] = "maxDimensions";

/// Audio constraint properties.
// Maximum supported sampling frequency (not necessarily ideal).
static constexpr char kMaxSampleRate[] = "maxSampleRate";
// Maximum number of audio channels (1 is mono, 2 is stereo, etc.).
static constexpr char kMaxChannels[] = "maxChannels";

/// Dimension properties.
// Width in pixels.
static constexpr char kWidth[] = "width";
// Height in pixels.
static constexpr char kHeight[] = "height";
// Frame rate as a rational decimal number or fraction.
// E.g. 30 and "3000/1001" are both valid representations.
static constexpr char kFrameRate[] = "frameRate";

/// Display description properties
// If this optional field is included in the ANSWER message, the receiver is
// attached to a fixed display that has the given dimensions and frame rate
// configuration. These may exceed, be the same, or be less than the values in
// constraints. If undefined, we assume the display is not fixed (e.g. a Google
// Hangouts UI panel).
static constexpr char kDimensions[] = "dimensions";
// An optional field. When missing and dimensions are specified, the sender
// will assume square pixels and the dimensions imply the aspect ratio of the
// fixed display. WHen present and dimensions are also specified, implies the
// pixels are not square.
static constexpr char kAspectRatio[] = "aspectRatio";
// The delimeter used for the aspect ratio format ("A:B").
static constexpr char kAspectRatioDelimiter[] = ":";
// Sets the aspect ratio constraints. Value must be either "sender" or
// "receiver", see kScalingSender and kScalingReceiver below.
static constexpr char kScaling[] = "scaling";
// Sender constraint means that the sender must scale and letterbox the content,
// and provide video frames of a FIXED aspect ratio.
static constexpr char kScalingSender[] = "sender";
// Receiver constraint means that the sender may send arbitrarily sized frames,
// and the receiver will handle scaling and letterboxing as necessary.
static constexpr char kScalingReceiver[] = "receiver";

/// Answer properties.
// A number specifying the UDP port used for all streams in this session.
// Must have a value between kUdpPortMin and kUdpPortMax.
static constexpr char kUdpPort[] = "udpPort";
static constexpr int kUdpPortMin = 1;
static constexpr int kUdpPortMax = 65535;
// Numbers specifying the indexes chosen from the offer message.
static constexpr char kSendIndexes[] = "sendIndexes";
// Numbers specifying the RTP SSRC values used to send the RTCP feedback of the
// stream indicated in kSendIndexes. Must have a value between kMinSsrc and
// kMaxSsrc.
static constexpr char kSsrcs[] = "ssrcs";
static constexpr int kMinSsrc = 0;
static constexpr int kMaxSsrc = 0xFFFFFFFF;
// Provides detailed maximum and minimum capabilities of the receiver for
// processing the selected streams. The sender may alter video resolution and
// frame rate throughout the session, and the constraints here determine how
// much data volume is allowed.
static constexpr char kConstraints[] = "constraints";
// Provides details about the display on the receiver.
static constexpr char kDisplay[] = "display";
// Optional array of numbers specifying the indexes of streams that will send
// event logs through RTCP.
static constexpr char kReceiverRtcpEventLog[] = "receiverRtcpEventLog";
// OPtional array of numbers specifying the indexes of streams that will use
// DSCP values specified in the OFFER message for RTCP packets.
static constexpr char kReceiverRtcpDscp[] = "receiverRtcpDscp";
// True if receiver can report wifi status.
static constexpr char kReceiverGetStatus[] = "receiverGetStatus";
// If this optional field is present the receiver supports the specific
// RTP extensions (such as adaptive playout delay).
static constexpr char kRtpExtensions[] = "rtpExtensions";

Json::Value AspectRatioConstraintToJson(AspectRatioConstraint aspect_ratio) {
  switch (aspect_ratio) {
    case AspectRatioConstraint::kVariable:
      return Json::Value(kScalingReceiver);
    case AspectRatioConstraint::kFixed:
    default:
      return Json::Value(kScalingSender);
  }
}

bool AspectRatioConstraintParseAndValidate(const Json::Value& value,
                                           AspectRatioConstraint* out) {
  std::string aspect_ratio;
  if (!ParseAndValidateString(value, &aspect_ratio)) {
    return false;
  }
  if (aspect_ratio == kScalingReceiver) {
    *out = AspectRatioConstraint::kVariable;
    return true;
  } else if (aspect_ratio == kScalingSender) {
    *out = AspectRatioConstraint::kFixed;
    return true;
  }
  return false;
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

template <typename T>
void ParseOptional(const Json::Value& value, Optional<T>* out) {
  T tentative_out;
  if (T::ParseAndValidate(value, &tentative_out)) {
    *out = Optional<T>(std::move(tentative_out));
  } else {
    *out = Optional<T>();
  }
}
}  // namespace

// static
bool AspectRatio::ParseAndValidate(const Json::Value& value, AspectRatio* out) {
  std::string parsed_value;
  if (!(ParseAndValidateString(value, &parsed_value))) {
    return false;
  }

  std::vector<absl::string_view> fields =
      absl::StrSplit(parsed_value, kAspectRatioDelimiter);
  if (fields.size() != 2) {
    return false;
  }

  if (!absl::SimpleAtoi(fields[0], &out->width) ||
      !absl::SimpleAtoi(fields[1], &out->height)) {
    return false;
  }
  return out->IsValid();
}

bool AspectRatio::IsValid() const {
  return width > 0 && height > 0;
}

// static
bool AudioConstraints::ParseAndValidate(const Json::Value& root,
                                        AudioConstraints* out) {
  if (!ParseAndValidateInt(root[kMaxSampleRate], &(out->max_sample_rate)) ||
      !ParseAndValidateInt(root[kMaxChannels], &(out->max_channels)) ||
      !ParseAndValidateInt(root[kMaxBitRate], &(out->max_bit_rate)) ||
      !ParseAndValidateMilliseconds(root[kMaxDelay], &(out->max_delay))) {
    return false;
  }
  if (!ParseAndValidateInt(root[kMinBitRate], &(out->min_bit_rate))) {
    out->min_bit_rate = kDefaultAudioMinBitRate;
  }
  return out->IsValid();
}

ErrorOr<Json::Value> AudioConstraints::ToJson() const {
  if (!IsValid()) {
    return Error::Code::kOperationInvalid;
  }

  Json::Value root;
  root[kMaxSampleRate] = max_sample_rate;
  root[kMaxChannels] = max_channels;
  root[kMinBitRate] = min_bit_rate;
  root[kMaxBitRate] = max_bit_rate;
  root[kMaxDelay] = Json::Value::Int64(max_delay.count());
  return root;
}

bool AudioConstraints::IsValid() const {
  return max_sample_rate > 0 && max_channels > 0 && min_bit_rate > 0 &&
         max_bit_rate >= min_bit_rate;
}

bool Dimensions::ParseAndValidate(const Json::Value& root, Dimensions* out) {
  if (!ParseAndValidateInt(root[kWidth], &(out->width)) ||
      !ParseAndValidateInt(root[kHeight], &(out->height)) ||
      !ParseAndValidateSimpleFraction(root[kFrameRate], &(out->frame_rate))) {
    return false;
  }
  return out->IsValid();
}

bool Dimensions::IsValid() const {
  return width > 0 && height > 0 && frame_rate.is_positive();
}

ErrorOr<Json::Value> Dimensions::ToJson() const {
  if (!IsValid()) {
    return CreateMessageError("Dimensions");
  }

  Json::Value root;
  root[kWidth] = width;
  root[kHeight] = height;
  root[kFrameRate] = frame_rate.ToString();
  return root;
}

// static
bool VideoConstraints::ParseAndValidate(const Json::Value& root,
                                        VideoConstraints* out) {
  if (!ParseAndValidateDouble(root[kMaxPixelsPerSecond],
                              &(out->max_pixels_per_second)) ||
      !Dimensions::ParseAndValidate(root[kMaxDimensions],
                                    &(out->max_dimensions)) ||
      !ParseAndValidateInt(root[kMaxBitRate], &(out->max_bit_rate)) ||
      !ParseAndValidateMilliseconds(root[kMaxDelay], &(out->max_delay))) {
    return false;
  }
  if (!ParseAndValidateInt(root[kMinBitRate], &(out->min_bit_rate))) {
    out->min_bit_rate = kDefaultVideoMinBitRate;
  }
  ParseOptional(root[kMinDimensions], &(out->min_dimensions));
  return out->IsValid();
}

bool VideoConstraints::IsValid() const {
  return max_pixels_per_second > 0 && min_bit_rate > 0 &&
         max_bit_rate > min_bit_rate && max_delay.count() > 0 &&
         max_dimensions.IsValid() &&
         (!min_dimensions || min_dimensions->IsValid()) &&
         max_dimensions.frame_rate.numerator > 0;
}

ErrorOr<Json::Value> VideoConstraints::ToJson() const {
  if (!IsValid()) {
    return CreateMessageError("VideoConstraints");
  }

  Json::Value min_dimensions_json;
  if (min_dimensions) {
    ErrorOr<Json::Value> error_or_min_dim = min_dimensions->ToJson();
    if (error_or_min_dim.is_error()) {
      return std::move(error_or_min_dim.error());
    }
    min_dimensions_json = std::move(error_or_min_dim.value());
  }

  ErrorOr<Json::Value> error_or_max_dim = max_dimensions.ToJson();
  if (error_or_max_dim.is_error()) {
    return std::move(error_or_max_dim.error());
  }

  Json::Value root;
  root[kMaxPixelsPerSecond] = max_pixels_per_second;
  root[kMinDimensions] = std::move(min_dimensions_json);
  root[kMaxDimensions] = error_or_max_dim.value();
  root[kMinBitRate] = min_bit_rate;
  root[kMaxBitRate] = max_bit_rate;
  root[kMaxDelay] = Json::Value::Int64(max_delay.count());
  return root;
}

// static
bool Constraints::ParseAndValidate(const Json::Value& root, Constraints* out) {
  if (!AudioConstraints::ParseAndValidate(root[kAudio], &(out->audio)) ||
      !VideoConstraints::ParseAndValidate(root[kVideo], &(out->video))) {
    return false;
  }
  return out->IsValid();
}

bool Constraints::IsValid() const {
  return audio.IsValid() && video.IsValid();
}

ErrorOr<Json::Value> Constraints::ToJson() const {
  ErrorOr<Json::Value> audio_or_error = audio.ToJson();
  if (audio_or_error.is_error()) {
    return std::move(audio_or_error.error());
  }

  ErrorOr<Json::Value> video_or_error = video.ToJson();
  if (video_or_error.is_error()) {
    return std::move(video_or_error.error());
  }

  Json::Value root;
  root[kAudio] = std::move(audio_or_error.value());
  root[kVideo] = std::move(video_or_error.value());
  return root;
}

// static
bool DisplayDescription::ParseAndValidate(const Json::Value& root,
                                          DisplayDescription* out) {
  ParseOptional(root[kDimensions], &(out->dimensions));
  ParseOptional(root[kAspectRatio], &(out->aspect_ratio));

  AspectRatioConstraint constraint;
  if (AspectRatioConstraintParseAndValidate(root[kScaling], &constraint)) {
    out->aspect_ratio_constraint =
        Optional<AspectRatioConstraint>(std::move(constraint));
  }

  return out->IsValid();
}

bool DisplayDescription::IsValid() const {
  // At least one of the properties must be set, and if a property is set
  // it must be valid.
  if (aspect_ratio && !aspect_ratio->IsValid()) {
    return false;
  }
  if (dimensions && !dimensions->IsValid()) {
    return false;
  }
  return aspect_ratio || dimensions || aspect_ratio_constraint;
}

ErrorOr<Json::Value> DisplayDescription::ToJson() const {
  if (!IsValid()) {
    return CreateMessageError("DisplayDescription");
  }

  Json::Value root;
  if (aspect_ratio) {
    root[kAspectRatio] = absl::StrCat(
        aspect_ratio->width, kAspectRatioDelimiter, aspect_ratio->height);
  }

  if (dimensions) {
    ErrorOr<Json::Value> dimensions_or_error = dimensions->ToJson();
    if (dimensions_or_error.is_error()) {
      return std::move(dimensions_or_error.error());
    }
    root[kDimensions] = dimensions_or_error.value();
  }

  if (aspect_ratio_constraint) {
    root[kScaling] =
        AspectRatioConstraintToJson(aspect_ratio_constraint.value());
  }

  return root;
}

bool Answer::ParseAndValidate(const Json::Value& root, Answer* out) {
  if (!ParseAndValidateInt(root[kUdpPort], &(out->udp_port)) ||
      !ParseAndValidateIntArray(root[kSendIndexes], &(out->send_indexes)) ||
      !ParseAndValidateUintArray(root[kSsrcs], &(out->ssrcs))) {
    return false;
  }
  if (!ParseBool(root[kReceiverGetStatus],
                 &(out->supports_wifi_status_reporting))) {
    out->supports_wifi_status_reporting = false;
  }

  // Constraints and display descriptions are optional fields, and maybe null in
  // the valid case.
  ParseOptional(root[kConstraints], &(out->constraints));
  ParseOptional(root[kDisplay], &(out->display));

  // These function set to empty array if not present, so we can ignore
  // the return value for optional values.
  ParseAndValidateIntArray(root[kReceiverRtcpEventLog],
                           &(out->receiver_rtcp_event_log));
  ParseAndValidateIntArray(root[kReceiverRtcpDscp], &(out->receiver_rtcp_dscp));
  ParseAndValidateStringArray(root[kRtpExtensions], &(out->rtp_extensions));

  return out->IsValid();
}

bool Answer::IsValid() const {
  if (ssrcs.empty() || send_indexes.empty()) {
    return false;
  }

  for (const uint32_t ssrc : ssrcs) {
    if (ssrc < kMinSsrc || ssrc > kMaxSsrc) {
      return false;
    }
  }
  // We don't know what the indexes used in the offer were here, so we just
  // sanity check.
  for (const int index : send_indexes) {
    if (index < 0) {
      return false;
    }
  }
  if (constraints && !constraints->IsValid()) {
    return false;
  }
  if (display && !display->IsValid()) {
    return false;
  }
  return kUdpPortMin <= udp_port && udp_port <= kUdpPortMax;
}

ErrorOr<Json::Value> Answer::ToJson() const {
  if (!IsValid()) {
    return CreateMessageError("Answer");
  }

  Json::Value root;
  if (constraints) {
    ErrorOr<Json::Value> constraints_or_error = constraints.value().ToJson();
    if (constraints_or_error.is_error()) {
      return std::move(constraints_or_error.error());
    }
    root[kConstraints] = std::move(constraints_or_error.value());
  }

  if (display) {
    ErrorOr<Json::Value> display_or_error = display.value().ToJson();
    if (display_or_error.is_error()) {
      return std::move(display_or_error.error());
    }
    root[kDisplay] = std::move(display_or_error.value());
  }

  root[kUdpPort] = udp_port;
  root[kReceiverGetStatus] = supports_wifi_status_reporting;
  root[kSendIndexes] = PrimitiveVectorToJson(send_indexes);
  root[kSsrcs] = PrimitiveVectorToJson(ssrcs);
  if (!receiver_rtcp_event_log.empty()) {
    root[kReceiverRtcpEventLog] =
        PrimitiveVectorToJson(receiver_rtcp_event_log);
  }
  if (!receiver_rtcp_dscp.empty()) {
    root[kReceiverRtcpDscp] = PrimitiveVectorToJson(receiver_rtcp_dscp);
  }
  if (!rtp_extensions.empty()) {
    root[kRtpExtensions] = PrimitiveVectorToJson(rtp_extensions);
  }
  return root;
}

Json::Value Answer::ToAnswerMessage() const {
  ErrorOr<Json::Value> json_or_error = ToJson();
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
