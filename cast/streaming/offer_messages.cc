// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/offer_messages.h"

#include <inttypes.h>

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "cast/streaming/constants.h"
#include "platform/base/error.h"
#include "util/big_endian.h"
#include "util/enum_name_table.h"
#include "util/json/json_helpers.h"
#include "util/json/json_serialization.h"
#include "util/osp_logging.h"
#include "util/stringprintf.h"

namespace openscreen {
namespace cast {

namespace {

constexpr char kSupportedStreams[] = "supportedStreams";
constexpr char kAudioSourceType[] = "audio_source";
constexpr char kVideoSourceType[] = "video_source";
constexpr char kStreamType[] = "type";

bool TryParseRtpPayloadType(const Json::Value& value, RtpPayloadType* out) {
  int t;
  if (!json::TryParseInt(value, &t)) {
    return false;
  }

  uint8_t t_small = t;
  if (t_small != t || !IsRtpPayloadType(t_small)) {
    return false;
  }

  *out = static_cast<RtpPayloadType>(t_small);
  return true;
}

bool TryParseRtpTimebase(const Json::Value& value, int* out) {
  std::string raw_timebase;
  if (!json::TryParseString(value, &raw_timebase)) {
    return false;
  }

  // The spec demands a leading 1, so this isn't really a fraction.
  const auto fraction = SimpleFraction::FromString(raw_timebase);
  if (fraction.is_error() || !fraction.value().is_positive() ||
      fraction.value().numerator() != 1) {
    return false;
  }

  *out = fraction.value().denominator();
  return true;
}

// For a hex byte, the conversion is 4 bits to 1 character, e.g.
// 0b11110001 becomes F1, so 1 byte is two characters.
constexpr int kHexDigitsPerByte = 2;
constexpr int kAesBytesSize = 16;
constexpr int kAesStringLength = kAesBytesSize * kHexDigitsPerByte;
bool TryParseAesHexBytes(const Json::Value& value,
                         std::array<uint8_t, kAesBytesSize>* out) {
  std::string hex_string;
  if (!json::TryParseString(value, &hex_string)) {
    return false;
  }

  constexpr int kHexDigitsPerScanField = 16;
  constexpr int kNumScanFields = kAesStringLength / kHexDigitsPerScanField;
  uint64_t quads[kNumScanFields];
  int chars_scanned;
  if (hex_string.size() == kAesStringLength &&
      sscanf(hex_string.c_str(), "%16" SCNx64 "%16" SCNx64 "%n", &quads[0],
             &quads[1], &chars_scanned) == kNumScanFields &&
      chars_scanned == kAesStringLength &&
      std::none_of(hex_string.begin(), hex_string.end(),
                   [](char c) { return std::isspace(c); })) {
    WriteBigEndian(quads[0], out->data());
    WriteBigEndian(quads[1], out->data() + 8);
    return true;
  }

  return false;
}

ErrorOr<Stream> TryParseStream(const Json::Value& value, Stream::Type type) {
  Stream stream;
  stream.type = type;

  if (!json::TryParseInt(value["index"], &stream.index) ||
      !json::TryParseUint(value["ssrc"], &stream.ssrc) ||
      !TryParseRtpPayloadType(value["rtpPayloadType"],
                              &stream.rtp_payload_type) ||
      !TryParseRtpTimebase(value["timeBase"], &stream.rtp_timebase)) {
    return Error(Error::Code::kJsonParseError,
                 "Offer stream has missing or invalid mandatory field");
  }

  if (!json::TryParseInt(value["channels"], &stream.channels)) {
    stream.channels = stream.type == Stream::Type::kAudioSource
                          ? kDefaultNumAudioChannels
                          : kDefaultNumVideoChannels;
  } else if (stream.channels <= 0) {
    return Error(Error::Code::kJsonParseError, "Invalid channel count");
  }

  if (!TryParseAesHexBytes(value["aesKey"], &stream.aes_key) ||
      !TryParseAesHexBytes(value["aesIvMask"], &stream.aes_iv_mask)) {
    return Error(Error::Code::kUnencryptedOffer,
                 "Offer stream must have both a valid aesKey and aesIvMask");
  }
  if (stream.rtp_timebase <
          std::min(kDefaultAudioMinSampleRate, kRtpVideoTimebase) ||
      stream.rtp_timebase > kRtpVideoTimebase) {
    return Error(Error::Code::kJsonParseError, "rtp_timebase (sample rate)");
  }

  stream.target_delay = kDefaultTargetPlayoutDelay;
  int target_delay;
  if (json::TryParseInt(value["targetDelay"], &target_delay)) {
    auto d = std::chrono::milliseconds(target_delay);
    if (kMinTargetPlayoutDelay <= d && d <= kMaxTargetPlayoutDelay) {
      stream.target_delay = d;
    }
  }

  if (!json::TryParseBool(value["receiverRtcpEventLog"],
                          &stream.receiver_rtcp_event_log)) {
    stream.receiver_rtcp_event_log = false;
  }
  if (!json::TryParseString(value["receiverRtcpDscp"],
                            &stream.receiver_rtcp_dscp)) {
    stream.receiver_rtcp_dscp = {};
  }

  return stream;
}

ErrorOr<AudioStream> TryParseAudioStream(const Json::Value& value) {
  AudioStream audio_stream;
  auto stream = TryParseStream(value, Stream::Type::kAudioSource);
  if (!stream) {
    return stream.error();
  }
  audio_stream.stream = std::move(stream.value());

  std::string codec_name;
  if (!json::TryParseInt(value["bitRate"], &audio_stream.bit_rate) ||
      audio_stream.bit_rate < 0 ||
      !json::TryParseString(value[kCodecName], &codec_name)) {
    return Error(Error::Code::kJsonParseError, "Invalid audio stream field");
  }
  ErrorOr<AudioCodec> codec = StringToAudioCodec(codec_name);
  if (!codec) {
    return Error(Error::Code::kUnknownCodec,
                 "Codec is not known, can't use stream");
  }
  audio_stream.codec = codec.value();
  return audio_stream;
}

bool TryParseResolutions(const Json::Value& value,
                         std::vector<Resolution>* out) {
  out->clear();

  // Some legacy senders don't provide resolutions, so just return empty.
  if (!value.isArray() || value.empty()) {
    return false;
  }

  for (Json::ArrayIndex i = 0; i < value.size(); ++i) {
    Resolution resolution;
    if (!Resolution::TryParse(value[i], &resolution)) {
      out->clear();
      return false;
    }
    out->push_back(std::move(resolution));
  }

  return true;
}

ErrorOr<VideoStream> TryParseVideoStream(const Json::Value& value) {
  VideoStream video_stream;
  auto stream = TryParseStream(value, Stream::Type::kVideoSource);
  if (!stream) {
    return stream.error();
  }
  video_stream.stream = std::move(stream.value());

  std::string codec_name;
  if (!json::TryParseString(value[kCodecName], &codec_name)) {
    return Error(Error::Code::kJsonParseError, "Video stream missing codec");
  }
  ErrorOr<VideoCodec> codec = StringToVideoCodec(codec_name);
  if (!codec) {
    return Error(Error::Code::kUnknownCodec,
                 "Codec is not known, can't use stream");
  }
  video_stream.codec = codec.value();

  video_stream.max_frame_rate = SimpleFraction{kDefaultMaxFrameRate, 1};
  std::string raw_max_frame_rate;
  if (json::TryParseString(value["maxFrameRate"], &raw_max_frame_rate)) {
    auto parsed = SimpleFraction::FromString(raw_max_frame_rate);
    if (parsed.is_value() && parsed.value().is_positive()) {
      video_stream.max_frame_rate = parsed.value();
    }
  }

  TryParseResolutions(value["resolutions"], &video_stream.resolutions);
  json::TryParseString(value["profile"], &video_stream.profile);
  json::TryParseString(value["protection"], &video_stream.protection);
  json::TryParseString(value["level"], &video_stream.level);
  json::TryParseString(value["errorRecoveryMode"],
                       &video_stream.error_recovery_mode);
  if (!json::TryParseInt(value["maxBitRate"], &video_stream.max_bit_rate)) {
    video_stream.max_bit_rate = 4 << 20;
  }

  return video_stream;
}

absl::string_view ToString(Stream::Type type) {
  switch (type) {
    case Stream::Type::kAudioSource:
      return kAudioSourceType;
    case Stream::Type::kVideoSource:
      return kVideoSourceType;
    default: {
      OSP_NOTREACHED();
    }
  }
}

EnumNameTable<CastMode, 2> kCastModeNames{
    {{"mirroring", CastMode::kMirroring}, {"remoting", CastMode::kRemoting}}};

}  // namespace

Json::Value Stream::ToJson() const {
  OSP_DCHECK(IsValid());

  Json::Value root;
  root["index"] = index;
  root["type"] = std::string(ToString(type));
  root["channels"] = channels;
  root["rtpPayloadType"] = static_cast<int>(rtp_payload_type);
  // rtpProfile is technically required by the spec, although it is always set
  // to cast. We set it here to be compliant with all spec implementers.
  root["rtpProfile"] = "cast";
  static_assert(sizeof(ssrc) <= sizeof(Json::UInt),
                "this code assumes Ssrc fits in a Json::UInt");
  root["ssrc"] = static_cast<Json::UInt>(ssrc);
  root["targetDelay"] = static_cast<int>(target_delay.count());
  root["aesKey"] = HexEncode(aes_key);
  root["aesIvMask"] = HexEncode(aes_iv_mask);
  root["receiverRtcpEventLog"] = receiver_rtcp_event_log;
  root["receiverRtcpDscp"] = receiver_rtcp_dscp;
  root["timeBase"] = "1/" + std::to_string(rtp_timebase);
  return root;
}

bool Stream::IsValid() const {
  return channels >= 1 && index >= 0 && target_delay.count() > 0 &&
         target_delay.count() <= std::numeric_limits<int>::max() &&
         rtp_timebase >= 1;
}

Json::Value AudioStream::ToJson() const {
  OSP_DCHECK(IsValid());

  Json::Value out = stream.ToJson();
  out[kCodecName] = CodecToString(codec);
  out["bitRate"] = bit_rate;
  return out;
}

bool AudioStream::IsValid() const {
  return bit_rate >= 0 && stream.IsValid();
}

Json::Value VideoStream::ToJson() const {
  OSP_DCHECK(IsValid());

  Json::Value out = stream.ToJson();
  out["codecName"] = CodecToString(codec);
  out["maxFrameRate"] = max_frame_rate.ToString();
  out["maxBitRate"] = max_bit_rate;
  out["protection"] = protection;
  out["profile"] = profile;
  out["level"] = level;
  out["errorRecoveryMode"] = error_recovery_mode;

  Json::Value rs;
  for (auto resolution : resolutions) {
    rs.append(resolution.ToJson());
  }
  out["resolutions"] = std::move(rs);
  return out;
}

bool VideoStream::IsValid() const {
  return max_bit_rate > 0 && max_frame_rate.is_positive();
}

// static
ErrorOr<Offer> Offer::TryParse(const Json::Value& root) {
  if (!root.isObject()) {
    return Error(Error::Code::kJsonParseError, "null offer");
  }
  const ErrorOr<CastMode> cast_mode =
      GetEnum(kCastModeNames, root["castMode"].asString());
  Json::Value supported_streams = root[kSupportedStreams];
  if (!supported_streams.isArray()) {
    return Error(Error::Code::kJsonParseError, "supported streams in offer");
  }

  std::vector<AudioStream> audio_streams;
  std::vector<VideoStream> video_streams;
  for (Json::ArrayIndex i = 0; i < supported_streams.size(); ++i) {
    const Json::Value& fields = supported_streams[i];
    std::string type;
    if (!json::TryParseString(fields[kStreamType], &type)) {
      return Error(Error::Code::kJsonParseError, "Missing stream type");
    }

    if (type == kAudioSourceType) {
      auto stream = TryParseAudioStream(fields);
      if (!stream) {
        if (stream.error().code() == Error::Code::kUnknownCodec) {
          OSP_DVLOG << "Dropping audio stream due to unknown codec: "
                    << stream.error();
          continue;
        } else {
          return stream.error();
        }
      }
      audio_streams.push_back(std::move(stream.value()));
    } else if (type == kVideoSourceType) {
      auto stream = TryParseVideoStream(fields);
      if (!stream) {
        if (stream.error().code() == Error::Code::kUnknownCodec) {
          OSP_DVLOG << "Dropping video stream due to unknown codec: "
                    << stream.error();
          continue;
        } else {
          return stream.error();
        }
      }
      video_streams.push_back(std::move(stream.value()));
    }
  }

  return Offer{cast_mode.value(CastMode::kMirroring), std::move(audio_streams),
               std::move(video_streams)};
}

Json::Value Offer::ToJson() const {
  OSP_DCHECK(IsValid());
  Json::Value root;
  root["castMode"] = GetEnumName(kCastModeNames, cast_mode).value();
  Json::Value streams;
  for (auto& stream : audio_streams) {
    streams.append(stream.ToJson());
  }

  for (auto& stream : video_streams) {
    streams.append(stream.ToJson());
  }

  root[kSupportedStreams] = std::move(streams);
  return root;
}

bool Offer::IsValid() const {
  return std::all_of(audio_streams.begin(), audio_streams.end(),
                     [](const AudioStream& a) { return a.IsValid(); }) &&
         std::all_of(video_streams.begin(), video_streams.end(),
                     [](const VideoStream& v) { return v.IsValid(); });
}
}  // namespace cast
}  // namespace openscreen
