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

// Although not needed by libcast's Sender and Receiver objects, certain
// properties such as the codec name are necessary for the embedder to properly
// use the library.
struct AudioConfig {
  AudioCodec codec;
  int bit_rate = 0;
};

// Display resolution in pixels.
struct DisplayResolution {
  int width = 0;
  int height = 0;
};

// Fields used by the embedder to send video. These fields are not used
// internally by libcast, and should be provided by the embedder.
struct VideoConfig {
  VideoCodec codec;
  double max_frame_rate = 0;
  int max_bit_rate = 0;
  std::string protection = {};
  std::string profile = {};
  std::string level = {};
  std::string error_recovery_mode = {};
  std::vector<DisplayResolution> resolutions = {};
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_SESSION_BASE_H_
