// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/offer.h"

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "platform/base/error.h"
#include "util/json/json_reader.h"
#include "util/logging.h"
#include <string>

namespace cast {
namespace streaming {

using openscreen::Error;
using openscreen::ErrorOr;

namespace {
// TODO(jophba): remove deprecated keys.
const char kAesIvMaskDeprecated[] = "aes-iv-mask";
const char kAesKeyDeprecated[] = "aes-key";
const char kAesIvMask[] = "aesIvMask";
const char kAesKey[] = "aesKey";
const char kAudioSource[] = "audio_source";
const char kBitrate[] = "bitRate";
const char kCastMode[] = "castMode";
const char kChannels[] = "channels";
const char kCastPliMessage[] = "PLI";
const char kCodecName[] = "codecName";
const char kErrorRecoveryMode[] = "errorRecoveryMode";
const char kHeight[] = "height";
const char kIncomingSsrc[] = "ssrc";
const char kOffer[] = "offer";
const char kReceiverGetStatus[] = "receiverGetStatus";
const char kReceiverRtcpDscp[] = "receiverRtcpDscp";
const char kReceiverRtcpEventLog[] = "receiverRtcpEventLog";
const char kResolutions[] = "resolutions";
const char kRtpExtensions[] = "rtpExtensions";
const char kRtpPayloadType[] = "rtpPayloadType";
const char kRtpProfile[] = "rtpProfile";
const char kSeqNum[] = "seqNum";
const char kStreamIndex[] = "index";
const char kStreamType[] = "type";
const char kSupportedStreams[] = "supportedStreams";
const char kTargetDelay[] = "targetDelay";
const char kTimeBase[] = "timeBase";
const char kUserAgent[] = "userAgent";
const char kVideoSource[] = "video_source";
const char kWidth[] = "width";

// Cast modes, default is "mirroring"
const char kCastMirroring[] = "mirroring";
const char kCastRemoting[] = "remoting";

template<std::size_t N>
Error HexKeyToByteArray(const Json::Value& key, std::array<uint8_t, N>* array) {
  if (!array) {
    return Error::Code::kParameterNullPointer;
  }
  if (N > sizeof(uint64_t)) {
    return Error::Code::kParameterOutOfRange;
  }

  const char* key_begin = nullptr;
  const char* key_end = nullptr;
  if (!key.getString(&key_begin, &key_end)) {
    return Error::Code::kParameterInvalid;
  }

  absl::string_view key_view(key_begin, key_end - key_begin);
  uint64_t byte_array = 0;
  constexpr int kHexBase = 16;
  if (!absl::safe_strtou64_base(key_view, &byte_array, kHexBase)) {
    return Error::Code::kParseError;
  }

  memcpy(array->data(), static_cast<const void*>(&byte_array), array->size());
}

ErrorOr<SessionConfig> ParseConfig(Json::Value config) {
  const std::string stream_type = config[kStreamType].asString();
  bool is_video;
  if (stream_type == kAudioSource) {
    is_video = false;
  } else if (stream_type == kVideoSource) {
    is_video = true;
  } else {
    return Error::Code::kJsonParseError;
  }

  const Ssrc incoming_ssrc = config[kIncomingSsrc].asUInt();

  // No longer needed
  // const uint32_t kRtpPayloadType = config[kRtpPayloadType].asUInt();

  // TODO(jophba): need deprecated versions?
  constexpr int kAesKeySize = 16;
  std::array<uint8_t, kAesKeySize> aes_secret_key{};
  const Error aes_key_error = HexKeyToByteArray(config[kAesKey], &aes_secret_key);
  if (!aes_key_error.ok()) {
    return aes_key_error;
  }

  std::array<uint8_t, kAesKeySize> aes_iv_mask{};
  const Error aes_iv_error = HexKeyToByteArray(config[kAesIvMask], &aes_iv_mask);
  if (!aes_iv_error.ok()) {
    return aes_iv_error;
  }

  // The RTP timebase is given as a frequency.
  const absl::string_view raw_time_base = config[kTimeBase].asCString();
  int rtp_timebase = 0;
  const char kTimeBasePrefix[] = "1/";
  if (!absl::StartsWith(raw_time_base, kTimeBasePrefix) ||
      !absl::SimpleAtoi(raw_time_base.substr(strlen(kTimeBase)),
                        &rtp_timebase)) {
    return Error::Code::kJsonParseError;
  }

  return SessionConfig{
      incoming_ssrc,
      incoming_ssrc,  // TODO: wat
      rtp_timebase,
      is_video ? 1 : 2,  // TODO: double wat
      std::move(aes_secret_key),
      std::move(aes_iv_mask),
  };
}

}  // namespace

// static
ErrorOr<Offer> Offer::Parse(Json::Value root) {
  const std::string cast_mode = root[kCastMode].asString();

  Offer::CastMode mode;
  if (cast_mode == kCastMirroring) {
    mode = Offer::CastMode::kMirroring;
  } else if (cast_mode == kCastRemoting) {
    mode = Offer::CastMode::kRemoting;
  } else {
    OSP_NOTREACHED() << "Unknown cast mode!";
    return;
  }

  Json::Value supported_streams = root[kSupportedStreams];
  if (!supported_streams.isArray()) {
    return Error::Code::kJsonParseError;
  }

  std::vector<SessionConfig> configs;
  for (Json::ArrayIndex i = 0; i < supported_streams.size(); ++i) {
    auto config = ParseConfig(supported_streams[i]);
    if (!config) {
      return config.error();
    }
    configs.push_back(std::move(config.value()));
  }

  // TODO: need kSeqNum??
  return Offer(mode, configs);
}

Offer::Offer(CastMode cast_mode, std::vector<SessionConfig> configs)
    : cast_mode_(cast_mode), configs_(configs) {}

}  // namespace streaming
}  // namespace cast