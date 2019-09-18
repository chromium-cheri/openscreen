// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_PLATFORM_API_VIDEO_ENCODER_H_
#define STREAMING_CAST_PLATFORM_API_VIDEO_ENCODER_H_

#include <memory>

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
  class Observer {
   public:
    virtual void OnEncodedFrame(std::unique_ptr<EncodedFrame> frame) = 0;
  };

  // TODO(jophba): what arguments does this need?
  // TODO(jophba): rename?
  static openscreen::ErrorOr<std::vector<EncoderCapabilities>>
  GetCapabilities();

  // Create an encoder with the name given by GetCapabilities and the attached
  // configuration.
  static openscreen::ErrorOr<VideoEncoder> Create(
      absl::string_view name,
      Environment* environment,
      const SenderConfiguration& sender_configuration,
      Observer* observer);

  // Encodes a video frame, running the frame_encoded_callback upon completion.
  // Returns a bool indicating whether the frame was accepted for processing, if
  // returns false then this is a noop.
  virtual bool Encode(std::unique_ptr<VideoFrame> frame,
                      openscreen::platform::Clock::time_point reference_time);

  // Set a new target bit rate.
  virtual void SetBitRate(int new_bit_rate) = 0;

  // Ask the encoder to encode a key frame soon. Depending on the encoder, this
  // may be the next frame or as it sees fit.
  virtual void RequestKeyFrame() = 0;

  // Flushes all currently in flight frames. This is especially useful under
  // network congestion.
  virtual void Flush() = 0;


  // TODO(jophba): clean up and add advanced
  // NOTES: QP only matters for VP8. vp8_encoder translates to 0.0 -> 1.0, and
  // there is a diff calculation for H264.
  // Probably have API return a ratio of "lossy_utilization"

  // https://cs.chromium.org/chromium/src/media/cast/sender/external_video_encoder.cc
  // Look in external_video_encoder, ParseVp8HeaderQuantizer. Could optionally
  // Add a quality parsing class that returns a quality value that can be
  // used to make decisions. Vp8 and H264 are the biggest codecs.

  // encoded_frame->lossy_utilization = perfect_quantizer / 63.0;
};

// TODO(jophba): define advanced feedback capabilities
}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_PLATFORM_API_VIDEO_ENCODER_H_
