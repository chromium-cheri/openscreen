// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_PLATFORM_API_AUDIO_ENCODER_H_
#define STREAMING_CAST_PLATFORM_API_AUDIO_ENCODER_H_

#include "absl/strings/string_view.h"
#include "platform/api/time.h"
#include "platform/base/error.h"
#include "streaming/cast/api/audio_frame.h"
#include "streaming/cast/encoded_frame.h"
#include "streaming/cast/encoder_capabilities.h"
#include "streaming/cast/environment.h"

namespace openscreen {
namespace cast_streaming {

class AudioEncoder {
 public:
  // TODO(jophba): define advanced encoder interface?
  class Observer {
   public:
    virtual void OnEncodedFrame(std::vector<AudioFrame>& source_frames,
                                EncodedFrame& encoded_frame) = 0;
  };

  // TODO(jophba): what arguments does this need?
  // TODO(jophba): rename?
  static openscreen::ErrorOr<std::vector<EncoderCapabilities>>
  GetCapabilities();

  // Create an encoder with the given name (provided by GetCapabilties) and
  // settings.
  static openscreen::ErrorOr<AudioEncoder> Create(absl::string_view name,
                                                  Environment& environment,
                                                  int num_channels,
                                                  int sampling_rate,
                                                  int bitrate,
                                                  Observer& observer);

  virtual void Encode(std::vector<AudioFrame>& rames,
                      platform::Clock::time_point recorded_time);
  virtual int GetSamplesPerFrame() const = 0;
  virtual platform::Clock::duration GetFrameDuration() const = 0;
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_PLATFORM_API_AUDIO_ENCODER_H_
