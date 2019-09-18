// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_PLATFORM_API_AUDIO_SENDER_H_
#define STREAMING_CAST_PLATFORM_API_AUDIO_SENDER_H_

#include "platform/base/error.h"
#include "streaming/cast/api/audio_bus.h"
#include "streaming/cast/api/sender_configuration.h"
#include "streaming/cast/api/sender_status.h"
#include "streaming/cast/environment.h"

namespace openscreen {
namespace cast_streaming {

class AudioSender {
 public:
  class Observer {
   public:
    virtual void OnStatusChange(SenderStatus status);
  };

  static ErrorOr<AudioSender> Create(Environment& environment,
                                     const SenderConfiguration& config,
                                     Observer& observer);

  void Insert(AudioBus& frames,
              platform::Clock::duration recorded_time);
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_PLATFORM_API_AUDIO_SENDER_H_
