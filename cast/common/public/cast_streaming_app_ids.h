// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_PUBLIC_CAST_STREAMING_APP_IDS_H_
#define CAST_COMMON_PUBLIC_CAST_STREAMING_APP_IDS_H_

#include <array>
#include <string>

namespace openscreen {
namespace cast {

constexpr char kChromeMirroringAppId[] = "0F5096E8";
constexpr char kChromeAudioMirroringAppId[] = "85CDB22F";
constexpr char kAndroidMirroringAppId[] = "674A0243";
constexpr char kAndroidAudioMirroringAppId[] = "8E6C866D";
constexpr char kAndroidAppMirroringAppId[] = "96084372";
constexpr char kIosAppMirroringAppId[] = "BFD92C23";

// Returns true only if |app_id| matches the Cast application ID for the
// corresponding Cast Streaming receiver application.
bool IsCastStreamingAppId(const std::string& app_id);
bool IsCastStreamingAudioVideoAppId(const std::string& app_id);
bool IsCastStreamingAudioOnlyAppId(const std::string& app_id);

// Returns all app IDs for Cast Streaming receivers.
constexpr std::array<const char*, 6> GetCastStreamingAppIds() {
  return {kChromeMirroringAppId,      kAndroidMirroringAppId,
          kAndroidAppMirroringAppId,  kIosAppMirroringAppId,
          kChromeAudioMirroringAppId, kAndroidAudioMirroringAppId};
}

// Returns the app ID for the audio and video streaming receiver.
constexpr std::array<const char*, 4> GetCastStreamingAudioVideoAppIds() {
  return {kChromeMirroringAppId, kAndroidMirroringAppId,
          kAndroidAppMirroringAppId, kIosAppMirroringAppId};
}

// Returns the app ID for the audio-only streaming receiver.
constexpr std::array<const char*, 2> GetCastStreamingAudioOnlyAppIds() {
  return {kChromeAudioMirroringAppId, kAndroidAudioMirroringAppId};
}

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_COMMON_PUBLIC_CAST_STREAMING_APP_IDS_H_
