// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/capture_configs.h"

#include <utility>

namespace openscreen::cast {

AudioCaptureConfig::AudioCaptureConfig(
    AudioCodec codec,
    int channels,
    int bit_rate,
    int sample_rate,
    std::chrono::milliseconds target_playout_delay,
    std::string codec_parameter)
    : codec(codec),
      channels(channels),
      bit_rate(bit_rate),
      sample_rate(sample_rate),
      target_playout_delay(target_playout_delay),
      codec_parameter(std::move(codec_parameter)) {}

AudioCaptureConfig::AudioCaptureConfig() = default;
AudioCaptureConfig::AudioCaptureConfig(const AudioCaptureConfig&) = default;
AudioCaptureConfig::AudioCaptureConfig(AudioCaptureConfig&&) noexcept = default;
AudioCaptureConfig& AudioCaptureConfig::operator=(const AudioCaptureConfig&) =
    default;
AudioCaptureConfig& AudioCaptureConfig::operator=(AudioCaptureConfig&&) =
    default;
AudioCaptureConfig::~AudioCaptureConfig() = default;

VideoCaptureConfig::VideoCaptureConfig(
    VideoCodec codec,
    SimpleFraction max_frame_rate,
    int max_bit_rate,
    std::vector<Resolution> resolutions,
    std::chrono::milliseconds target_playout_delay,
    std::string codec_parameter)
    : codec(codec),
      max_frame_rate(std::move(max_frame_rate)),
      max_bit_rate(max_bit_rate),
      resolutions(std::move(resolutions)),
      target_playout_delay(target_playout_delay),
      codec_parameter(std::move(codec_parameter)) {}

VideoCaptureConfig::VideoCaptureConfig() = default;
VideoCaptureConfig::VideoCaptureConfig(const VideoCaptureConfig&) = default;
VideoCaptureConfig::VideoCaptureConfig(VideoCaptureConfig&&) noexcept = default;
VideoCaptureConfig& VideoCaptureConfig::operator=(const VideoCaptureConfig&) =
    default;
VideoCaptureConfig& VideoCaptureConfig::operator=(VideoCaptureConfig&&) =
    default;
VideoCaptureConfig::~VideoCaptureConfig() = default;

}  // namespace openscreen::cast
