// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_SESSION_BASE_H_
#define CAST_STREAMING_SESSION_BASE_H_

#include <string>
#include <vector>

namespace openscreen {
namespace cast {

// Codecs known and understood by cast senders and receivers.
// Note: embedders are required to implement the following
// codecs to be Cast V2 compliant: H264, VP8, AAC, Opus.
enum class AudioCodec { kAac, kOpus };
enum class VideoCodec { kH264, kVp8, kHevc, kVp9 };

std::string CodecToString(AudioCodec codec);
std::string CodecToString(VideoCodec codec);

// Although not needed by libcast's Sender and Receiver objects, certain
// properties such as the codec name are necessary for the embedder to properly
// use the library.
struct AudioConfig {
  // Audio codec represented by this configuration. Mandatory field.
  AudioCodec codec;

  // Number of channels used by this configuration. Mandatory field.
  int channels = 2;

  // Average bit rate in bits per second used by this configuration. Mandatory
  // field.
  int bit_rate = 0;
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

// Fields used by the embedder to send video. These fields are not used
// internally by libcast, and should be provided by the embedder.
struct VideoConfig {
  // Video codec represented by this configuration. Mandatory field.
  VideoCodec codec;

  // Maximum frame rate in frames per second. Mandatory field.
  FrameRate max_frame_rate = {};

  // Number specifying the max bit rate for theis stream. Mandatory field.
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
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_SESSION_BASE_H_
