// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/offer_messages.h"

#include <inttypes.h>

#include <string>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "platform/base/error.h"
#include "util/big_endian.h"
#include "util/json/json_reader.h"
#include "util/logging.h"

namespace cast {
namespace streaming {

using openscreen::Error;
using openscreen::ErrorOr;

namespace {

// Cast modes, default is "mirroring"
const char kCastMirroring[] = "mirroring";
const char kCastMode[] = "castMode";
const char kCastRemoting[] = "remoting";

const char kSupportedStreams[] = "supportedStreams";
const char kAudioSourceType[] = "audio_source";
const char kVideoSourceType[] = "video_source";
const char kStreamType[] = "type";

ErrorOr<bool> ParseBool(const Json::Value& value) {
  if (!value.isBool()) {
    return Error::Code::kJsonParseError;
  }
  return value.asBool();
}

ErrorOr<int> ParseInt(const Json::Value& value) {
  if (!value.isInt()) {
    return Error::Code::kJsonParseError;
  }
  return value.asInt();
}

ErrorOr<uint32_t> ParseUint(const Json::Value& value) {
  if (!value.isUInt()) {
    return Error::Code::kJsonParseError;
  }
  return value.asUInt();
}

ErrorOr<std::string> ParseString(const Json::Value& value) {
  if (!value.isString()) {
    return Error::Code::kJsonParseError;
  }
  return value.asString();
}

// Use this template for parsing only when there is a reasonable default
// for the type you are using, e.g. int or std::string.
template <typename T>
T ValueOrDefault(const ErrorOr<T>& value) {
  if (value.is_value()) {
    return value.value();
  }
  return T{};
}

ErrorOr<int> ParseRtpTimebase(const Json::Value& value) {
  auto error_or_raw = ParseString(value);
  if (!error_or_raw) {
    return error_or_raw.error();
  }

  int rtp_timebase = 0;
  const char kTimeBasePrefix[] = "1/";
  if (!absl::StartsWith(error_or_raw.value(), kTimeBasePrefix) ||
      !absl::SimpleAtoi(error_or_raw.value().substr(strlen(kTimeBasePrefix)),
                        &rtp_timebase) ||
      (rtp_timebase <= 0)) {
    return Error::Code::kJsonParseError;
  }

  return rtp_timebase;
}

ErrorOr<std::array<uint8_t, 16>> ParseAesHexBytes(const Json::Value& value) {
  auto hex_string = ParseString(value);
  if (!hex_string) {
    return hex_string.error();
  }

  uint64_t quads[2];
  int chars_scanned;
  if (hex_string.value().size() == 32 &&
      sscanf(hex_string.value().c_str(), "%16" SCNx64 "%16" SCNx64 "%n",
             &quads[0], &quads[1], &chars_scanned) == 2 &&
      chars_scanned == 32 &&
      std::none_of(hex_string.value().begin(), hex_string.value().end(),
                   [](char c) { return std::isspace(c); })) {
    std::array<uint8_t, 16> bytes;
    openscreen::WriteBigEndian(quads[0], bytes.data());
    openscreen::WriteBigEndian(quads[1], bytes.data() + 8);
    return bytes;
  }
  return Error::Code::kJsonParseError;
}

ErrorOr<Stream> ParseStream(const Json::Value& value, Stream::Type type) {
  auto index = ParseInt(value["index"]);
  auto codec_name = ParseString(value["codecName"]);
  auto rtp_profile = ParseString(value["rtpProfile"]);
  auto rtp_payload_type = ParseInt(value["rtpPayloadType"]);
  auto ssrc = ParseUint(value["ssrc"]);
  auto target_delay = ParseInt(value["targetDelay"]);
  auto aes_key = ParseAesHexBytes(value["aesKey"]);
  auto aes_iv_mask = ParseAesHexBytes(value["aesIvMask"]);
  auto receiver_rtcp_event_log = ParseBool(value["receiverRtcpEventLog"]);
  auto receiver_rtcp_dscp = ParseString(value["receiverRtcpDscp"]);
  auto time_base = ParseRtpTimebase(value["timeBase"]);

  if (!index || !codec_name || !rtp_profile || !rtp_payload_type || !ssrc ||
      !time_base) {
    OSP_LOG_ERROR << "Missing required stream property.";
    return Error::Code::kJsonParseError;
  }

  if (rtp_profile.value() != "cast") {
    OSP_LOG_ERROR << "Received invalid RTP profile: " << rtp_profile;
  }

  if (rtp_payload_type.value() < 96 || rtp_payload_type.value() > 127) {
    OSP_LOG_ERROR << "Received invalid RTP payload type: "
                  << rtp_payload_type.value();
    return Error::Code::kParameterInvalid;
  }

  if (time_base.value() <= 0) {
    OSP_LOG_ERROR << "Received invalid RTP timebase: " << time_base.value();
    return Error::Code::kParameterInvalid;
  }

  std::chrono::milliseconds target_delay_ms = kDefaultTargetDelay;
  if (target_delay) {
    auto d = std::chrono::milliseconds(target_delay.value());
    if (d >= kMinTargetDelay && d <= kMaxTargetDelay) {
      target_delay_ms = d;
    }
  }

  return Stream{index.value(),
                type,
                codec_name.value(),
                RtpProfile::kCast,
                rtp_payload_type.value(),
                ssrc.value(),
                target_delay_ms,
                ValueOrDefault(aes_key),
                ValueOrDefault(aes_iv_mask),
                ValueOrDefault(receiver_rtcp_event_log),
                ValueOrDefault(receiver_rtcp_dscp),
                time_base.value()};
}

ErrorOr<AudioStream> ParseAudioStream(const Json::Value& value) {
  auto stream = ParseStream(value, Stream::Type::kAudioSource);
  auto bit_rate = ParseInt(value["bitRate"]);
  auto channels = ParseInt(value["channels"]);

  if (!stream || !bit_rate || !channels) {
    return Error::Code::kJsonParseError;
  }

  if (bit_rate.value() <= 0) {
    OSP_LOG_ERROR << "Received non-positive audio bitrate: "
                  << bit_rate.value();
    return Error::Code::kParameterInvalid;
  }
  return AudioStream{stream.value(), bit_rate.value(), channels.value()};
}

ErrorOr<Resolution> ParseResolution(const Json::Value& value) {
  auto width = ParseInt(value["width"]);
  auto height = ParseInt(value["height"]);

  if (!width || !height) {
    return Error::Code::kJsonParseError;
  }

  return Resolution{width.value(), height.value()};
}

ErrorOr<std::vector<Resolution>> ParseResolutions(const Json::Value& value) {
  if (!value.isArray()) {
    return Error::Code::kJsonParseError;
  }

  std::vector<Resolution> resolutions;
  if (value.empty()) {
    return resolutions;
  }

  for (unsigned int i = 0; i < value.size(); ++i) {
    auto r = ParseResolution(value[i]);
    if (!r) {
      return r.error();
    }
    resolutions.push_back(r.value());
  }

  return resolutions;
}

ErrorOr<VideoStream> ParseVideoStream(const Json::Value& value) {
  auto stream = ParseStream(value, Stream::Type::kVideoSource);
  auto max_frame_rate = ParseString(value["maxFrameRate"]);
  auto max_bit_rate = ParseInt(value["maxBitRate"]);
  auto protection = ParseString(value["protection"]);
  auto profile = ParseString(value["profile"]);
  auto level = ParseString(value["level"]);
  auto resolutions = ParseResolutions(value["resolutions"]);
  auto error_recovery_mode = ParseString(value["errorRecoveryMode"]);

  if (!stream || !max_bit_rate || !resolutions) {
    return Error::Code::kJsonParseError;
  }

  return VideoStream{stream.value(),
                     ValueOrDefault(max_frame_rate),
                     ValueOrDefault(max_bit_rate),
                     ValueOrDefault(protection),
                     ValueOrDefault(profile),
                     ValueOrDefault(level),
                     resolutions.value(),
                     ValueOrDefault(error_recovery_mode)};
}

ErrorOr<Offer::CastMode> ParseCastMode(const Json::Value& value) {
  auto cast_mode = ParseString(value);
  if (!cast_mode) {
    OSP_LOG_ERROR << "Received no cast mode";
    return Error::Code::kJsonParseError;
  }

  if (cast_mode.value() == kCastMirroring) {
    return Offer::CastMode::kMirroring;
  }
  if (cast_mode.value() == kCastRemoting) {
    return Offer::CastMode::kRemoting;
  }

  OSP_LOG_ERROR << "Received invalid cast mode: " << cast_mode.value();
  return Error::Code::kJsonParseError;
}
}  // namespace

// static
ErrorOr<Offer> Offer::Parse(const Json::Value& root) {
  auto cast_mode = ParseCastMode(root[kCastMode]);
  if (!cast_mode) {
    return Error::Code::kJsonParseError;
  }

  Json::Value supported_streams = root[kSupportedStreams];
  if (!supported_streams.isArray()) {
    return Error::Code::kJsonParseError;
  }

  std::vector<AudioStream> audio_streams;
  std::vector<VideoStream> video_streams;
  for (Json::ArrayIndex i = 0; i < supported_streams.size(); ++i) {
    auto type = ParseString(supported_streams[i][kStreamType]);
    if (!type) {
      OSP_LOG_ERROR << "Stream missing mandatory type field.";
      return Error::Code::kJsonParseError;
    }
    if (type.value() == kAudioSourceType) {
      auto stream = ParseAudioStream(supported_streams[i]);
      if (!stream) {
        OSP_LOG_ERROR << "Failed to parse audio stream.";
        return Error::Code::kJsonParseError;
      }
      audio_streams.push_back(std::move(stream.value()));
    } else if (type.value() == kVideoSourceType) {
      auto stream = ParseVideoStream(supported_streams[i]);
      if (!stream) {
        OSP_LOG_ERROR << "Failed to parse video stream.";
        return Error::Code::kJsonParseError;
      }
      video_streams.push_back(std::move(stream.value()));
    }
  }

  return Offer{cast_mode.value(), std::move(audio_streams),
               std::move(video_streams)};
}

}  // namespace streaming
}  // namespace cast