// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_PLATFORM_API_AUDIO_ENCODER_H_
#define STREAMING_CAST_PLATFORM_API_AUDIO_ENCODER_H_

#include "absl/strings/string_view.h"
#include "platform/api/time.h"
#include "platform/base/error.h"
#include "streaming/cast/environment.h"
#include "streaming/cast/platform/base/encoder_capabilities.h"

namespace openscreen {
namespace cast_streaming {

class AudioEncoder {
 public:
  class Observer {
    virtual void OnEncodedFrame(std::unique_ptr<EncodedFrame> frame) = 0;
  };

  // TODO(jophba): what arguments does this need?
  // TODO(jophba): rename?
  static openscreen::ErrorOr<std::vector<EncoderCapabilities>>
  GetCapabilities();

  // Create an encoder with the given name (provided by GetCapabilties) and
  // settings.
  static openscreen::ErrorOr<AudioEncoder> Create(absl::string_view name,
                                                  Environment* environment,
                                                  int num_channels,
                                                  int sampling_rate,
                                                  int bitrate,
                                                  Observer* observer);
}

// TODO(jophba): define advanced encoder interface
// TODO(jophba): add capabilities

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_PLATFORM_API_AUDIO_ENCODER_H_
