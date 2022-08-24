// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/receiver_preferences.h"

#include <utility>

namespace openscreen {
namespace cast {

ReceiverPreferences::ReceiverPreferences() = default;
ReceiverPreferences::ReceiverPreferences(std::vector<VideoCodec> video_codecs,
                                         std::vector<AudioCodec> audio_codecs)
    : video_codecs(std::move(video_codecs)),
      audio_codecs(std::move(audio_codecs)) {}

ReceiverPreferences::ReceiverPreferences(std::vector<VideoCodec> video_codecs,
                                         std::vector<AudioCodec> audio_codecs,
                                         std::vector<AudioLimits> audio_limits,
                                         std::vector<VideoLimits> video_limits,
                                         std::unique_ptr<Display> description)
    : video_codecs(std::move(video_codecs)),
      audio_codecs(std::move(audio_codecs)),
      audio_limits(std::move(audio_limits)),
      video_limits(std::move(video_limits)),
      display_description(std::move(description)) {}

ReceiverPreferences::ReceiverPreferences(ReceiverPreferences&&) noexcept =
    default;
ReceiverPreferences& ReceiverPreferences::operator=(
    ReceiverPreferences&&) noexcept = default;

ReceiverPreferences::ReceiverPreferences(const ReceiverPreferences& other) {
  *this = other;
}

ReceiverPreferences& ReceiverPreferences::operator=(
    const ReceiverPreferences& other) {
  video_codecs = other.video_codecs;
  audio_codecs = other.audio_codecs;
  audio_limits = other.audio_limits;
  video_limits = other.video_limits;
  if (other.display_description) {
    display_description = std::make_unique<Display>(*other.display_description);
  }
  if (other.remoting) {
    remoting = std::make_unique<RemotingPreferences>(*other.remoting);
  }
  return *this;
}

}  // namespace cast
}  // namespace openscreen
