// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_PLATFORM_API_VIDEO_ENCODER_H_
#define STREAMING_CAST_PLATFORM_API_VIDEO_ENCODER_H_

#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "platform/api/time.h"
#include "platform/base/error.h"
#include "streaming/cast/api/sender_configuration.h"
#include "streaming/cast/api/video_frame.h"
#include "streaming/cast/api/video_sender.h"
#include "streaming/cast/encoded_frame.h"
#include "streaming/cast/encoder_capabilities.h"
#include "streaming/cast/environment.h"

namespace openscreen {
namespace cast_streaming {

class VideoEncoder {
  // Encoders may, and are not required to, provide statistics to help the
  // calling code make decisions about how to send content.
  struct Statistics {
    // A normalized value indicating how close to the ideal (1.0) utilization
    // the encoder was during this frame. Typically, encoders define this value
    // as (actual bit rate / target bit rate) * (actual qp / max qp), however
    // encoders are free to define this value as they see fit.
    // NOTE: probably don't need a statistics class if we go this route.
    double effective_utilization;
  };

  class Observer {
   public:
    virtual void OnEncodedFrame(VideoFrame& original_frame,
                                std::unique_ptr<EncodedFrame> encoded_frame,
                                absl::optional<Statistics> stats) = 0;
  };

  // TODO(jophba): what arguments does this need?
  // TODO(jophba): rename?
  static openscreen::ErrorOr<std::vector<EncoderCapabilities>>
  GetCapabilities();

  // Create an encoder with the name given by GetCapabilities and the attached
  // configuration.
  static openscreen::ErrorOr<VideoEncoder> Create(
      absl::string_view name,
      Environment& environment,
      const SenderConfiguration& sender_configuration,
      Observer& observer);

  // Encodes a video frame, running the frame_encoded_callback upon completion.
  // Returns a bool indicating whether the frame was accepted for processing, if
  // returns false then this is a noop.
  virtual bool Encode(VideoFrame& frame,
                      openscreen::platform::Clock::time_point reference_time);

  // Set a new target bit rate.
  virtual void SetBitRate(int new_bit_rate) = 0;

  // Ask the encoder to encode a key frame soon. Depending on the encoder, this
  // may be the next frame or as it sees fit.
  virtual void RequestKeyFrame() = 0;

  // Flushes all currently in flight frames. This is especially useful under
  // network congestion.
  virtual void Flush() = 0;
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_PLATFORM_API_VIDEO_ENCODER_H_
