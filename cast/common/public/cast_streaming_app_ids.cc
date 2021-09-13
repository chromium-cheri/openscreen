// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/public/cast_streaming_app_ids.h"

// Cast Application ID for the audio+video streaming receiver application.
constexpr char kCastStreamingAudioVideoAppId[] = "0F5096E8";

// Cast Application ID for the audio-only streaming receiver application.
constexpr char kCastStreamingAudioOnlyAppId[] = "85CDB22F";

namespace openscreen {
namespace cast {

bool IsCastStreamingAppId(const std::string& app_id) {
  return IsCastStreamingAudioOnlyAppId(app_id) ||
         IsCastStreamingAudioVideoAppId(app_id);
}

bool IsCastStreamingAudioVideoAppId(const std::string& app_id) {
  return app_id == kCastStreamingAudioVideoAppId;
}

bool IsCastStreamingAudioOnlyAppId(const std::string& app_id) {
  return app_id == kCastStreamingAudioOnlyAppId;
}

std::vector<std::string> GetCastStreamingAppIds() {
  return std::vector<std::string>(
      {kCastStreamingAudioVideoAppId, kCastStreamingAudioOnlyAppId});
}

const char* GetCastStreamingAudioVideoAppId() {
  return kCastStreamingAudioVideoAppId;
}

const char* GetCastStreamingAudioOnlyAppId() {
  return kCastStreamingAudioOnlyAppId;
}

}  // namespace cast
}  // namespace openscreen
