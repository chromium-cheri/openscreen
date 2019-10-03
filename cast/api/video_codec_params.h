// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_API_VIDEO_CODEC_PARAMS_H_
#define CAST_API_VIDEO_CODEC_PARAMS_H_

#include <utility>

#include "absl/types/optional.h"

namespace cast {
namespace api {

// Parameters specific to video codecs. Most standard codecs use a quantization
// strategy, so it is assumed that they will have quantization parameters (QPs).
// If a video codec is added without QPs, they may omit this optional struct
// from their configurations.
struct VideoCodecParams {
  // The min and max quantization parameter limits. Codecs such as H.264 use
  // this for deriving a scaling matrix.
  std::pair<int, int> quantization_parameter_limits;

  // The encoder quantization parameter to use when CPU is constrained. This
  // value should represent a tradeoff between higher resolution and higher
  // encoding quality. Value must be in bounds of quantization_parameter_limits.
  int max_cpu_save_quantization_parameter;

  // One some encoders, this value is used to control the max frames in flight
  // in the encoder. A larger window allows for higher efficiency at the cost
  // of higher memory usage and latency. A value of "0" uses the encoder's
  // default values.
  absl::optional<int> max_num_video_buffers;

  // Number of threads to use for encoding.
  absl::optional<int> num_encode_threads;
};

}  // namespace api
}  // namespace cast

#endif  // CAST_API_VIDEO_CODEC_PARAMS_H_
