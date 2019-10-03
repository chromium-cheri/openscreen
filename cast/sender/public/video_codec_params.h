// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_SENDER_PUBLIC_VIDEO_CODEC_PARAMS_H_
#define CAST_SENDER_PUBLIC_VIDEO_CODEC_PARAMS_H_

#include <utility>

namespace cast {
namespace api {

struct QuantizationParameter {
 public:
  explicit QuantizationParameter(int qp_in) : qp(qp_in) {
    OSP_DCHECK(qp > 0) << "Quantization parameters must be greater than zero";
  }

  int qp;
};

// Parameters specific to video codecs. Most standard codecs use a quantization
// strategy, so it is assumed that they will have quantization parameters (QPs).
// If a video codec is added without QPs, they may omit this optional struct
// from their configurations.
struct VideoCodecParams {
  // The min and max quantization parameter limits. Codecs such as H.264 use
  // this for deriving a scaling matrix.
  std::pair<QuantizationParameter, QuantizationParameter>
      quantization_parameter_limits;

  // The encoder quantization parameter to use when CPU is constrained. This
  // value should represent a tradeoff between higher resolution and higher
  // encoding quality. Value must be in bounds of quantization_parameter_limits.
  QuantizationParameter max_cpu_save_quantization_parameter;

  // One some encoders, this value is used to control the max frames in flight
  // in the encoder. A larger window allows for higher efficiency at the cost
  // of higher memory usage and latency. If value is 0, the default
  // is used.
  int max_num_video_buffers = 0;

  // Number of threads to use for encoding. If value is 0, the
  // default is used.
  int num_encode_threads = 0;
};

}  // namespace api
}  // namespace cast

#endif  // CAST_SENDER_PUBLIC_VIDEO_CODEC_PARAMS_H_
