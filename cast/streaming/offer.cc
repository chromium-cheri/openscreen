// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/offer.h"

#include <inttypes.h>

#include <string>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "platform/base/error.h"
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
const char kAudioSource[] = "audio_source";
const char kVideoSource[] = "video_source";
const char kStreamType[] = "type";

}  // namespace

Stream::Stream(Json::Value stream) : stream_(stream) {}
Stream::~Stream() = default;

Resolution::Resolution(Json::Value resolution) : resolution_(resolution) {}
ResolutionList::ResolutionList(Json::Value resolution_list)
    : resolution_list_(resolution_list) {}
AudioStream::AudioStream(Json::Value stream) : Stream(std::move(stream)) {}
VideoStream::VideoStream(Json::Value stream) : Stream(std::move(stream)) {}

// static
ErrorOr<Offer> Offer::Parse(Json::Value root) {
  const std::string cast_mode = root[kCastMode].asString();

  CastMode mode;
  if (cast_mode == kCastMirroring) {
    mode = CastMode::kMirroring;
  } else if (cast_mode == kCastRemoting) {
    mode = CastMode::kRemoting;
  } else {
    OSP_NOTREACHED() << "Unknown cast mode!";
    return Error::Code::kParameterInvalid;
  }

  Json::Value supported_streams = root[kSupportedStreams];
  if (!supported_streams.isArray()) {
    return Error::Code::kJsonParseError;
  }

  std::vector<AudioStream> audio_streams;
  std::vector<VideoStream> video_streams;
  for (Json::ArrayIndex i = 0; i < supported_streams.size(); ++i) {
    std::string type = supported_streams[i][kStreamType].asString();
    if (type == kAudioSource) {
      audio_streams.emplace_back(std::move(supported_streams[i]));
    } else if (type == kVideoSource) {
      video_streams.emplace_back(std::move(supported_streams[i]));
    }
  }

  return Offer(mode, audio_streams, video_streams);
}

Offer::Offer(CastMode cast_mode,
             std::vector<AudioStream> audio_streams,
             std::vector<VideoStream> video_streams)
    : cast_mode_(cast_mode),
      audio_streams_(audio_streams),
      video_streams_(video_streams) {}

}  // namespace streaming
}  // namespace cast