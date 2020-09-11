// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_CAPTURE_OPTIONS_H_
#define CAST_STREAMING_CAPTURE_OPTIONS_H_

#include <string>
#include <vector>

#include "cast/streaming/constants.h"

namespace openscreen {
namespace cast {

std::string CodecToString(AudioCodec codec);
std::string CodecToString(VideoCodec codec);

// A configuration set that can be used by the sender to capture audio. Used
// by Cast Streaming to provide an offer to the receiver.
struct AudioCaptureOption {
  // Audio codec represented by this configuration. Mandatory field.
  AudioCodec codec = AudioCodec::kOpus;

  // Number of channels used by this configuration. Mandatory field.
  int channels = 2;

  // Average bit rate in bits per second used by this configuration. Mandatory
  // field.
  int bit_rate = 0;

  // Sample rate for audio RTP timebase. Mandatory field.
  int sample_rate = 0;

  // Target playout delay in milliseconds.
  std::chrono::milliseconds target_playout_delay = kDefaultTargetPlayoutDelay;
};

// Display resolution in pixels.
struct DisplayResolution {
  int width = 0;
  int height = 0;
};

// Frame rates are expressed as a rational number, and must be positive.
struct FrameRate {
  int numerator = 60;
  int denominator = 1;
};

// A configuration set that can be used by the sender to capture video. Used
// by Cast Streaming to provide an offer to the receiver.
struct VideoCaptureOption {
  // Video codec represented by this configuration. Mandatory field.
  VideoCodec codec = VideoCodec::kVp8;

  // Maximum frame rate in frames per second. Mandatory field.
  FrameRate max_frame_rate = {};

  // Number specifying the max bit rate for this stream. Mandatory field.
  int max_bit_rate = 0;

  // Some video streams have additional protection, typically for handling
  // errors in transmission, such as FEC. Optional field.
  std::string protection = {};

  // Some codecs define a profile, which limits the codecs capabilities and
  // determines what settings it uses. Optional field.
  std::string profile = {};

  // Some codec standard define a level, which limits the combination
  // of resolution, frame rate, and bitrate. Optional field.
  std::string level = {};

  // Resolutions to be offered to the receiver. Mandatory field, must have
  // at least one resolution provided.
  std::vector<DisplayResolution> resolutions = {};

  // Target playout delay in milliseconds.
  std::chrono::milliseconds target_playout_delay = kDefaultTargetPlayoutDelay;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_CAPTURE_OPTIONS_H_
