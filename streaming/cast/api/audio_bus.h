// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_PLATFORM_API_AUDIO_BUS_H_
#define STREAMING_CAST_PLATFORM_API_AUDIO_BUS_H_

#include <vector>

#include "streaming/cast/api/audio_frame.h"

namespace openscreen {
namespace cast_streaming {

// TODO(jophba): fill out
class AudioBus {
  explicit AudioBus(std::vector<AudioFrame> frames) : frames_(frames) {}

 private:
  std::vector<AudioFrame> frames_;
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_PLATFORM_API_AUDIO_BUS_H_
