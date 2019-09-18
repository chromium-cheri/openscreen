// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_PLATFORM_API_VIDEO_SENDER_H_
#define STREAMING_CAST_PLATFORM_API_VIDEO_SENDER_H_

#include <unordered_map>
#include <utility>

#include "platform/api/time.h"
#include "streaming/cast/api/sender_configuration.h"
#include "streaming/cast/api/sender_status.h"
#include "streaming/cast/api/video_codec_params.h"
#include "streaming/cast/api/video_frame.h"
#include "streaming/cast/environment.h"

namespace openscreen {
namespace cast_streaming {

class VideoSender {
 public:
  class Observer {
   public:
    virtual void OnPlaybackDelayChange(platform::Clock::duration delay) = 0;
    virtual void OnStatusChange(SenderStatus new_status) = 0;
  };

  static ErrorOr<VideoSender> Create(Environment* environment,
                                     const SenderConfiguration& configuration,
                                     // TODO(jophba): add encode callbacks??
                                     // NOTE: CastTransport is not exposed here.
                                     Observer* observer);

  // Inserts a frame. Note: there is no guarantee that the frame will actually
  // be sent in case of throttling.
  virtual void Insert(std::unique_ptr<VideoFrame> frame) = 0;
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_PLATFORM_API_VIDEO_SENDER_H_
