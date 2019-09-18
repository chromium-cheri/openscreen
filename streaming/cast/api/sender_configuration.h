
// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_PLATFORM_API_SENDER_CONFIGURATION_H_
#define STREAMING_CAST_PLATFORM_API_SENDER_CONFIGURATION_H_

#include <string>
#include <unordered_map>
#include <utility>

#include "platform/api/time.h"
#include "streaming/cast/api/video_codec_params.h"
#include "streaming/cast/rtp_defines.h"

namespace openscreen {
namespace cast_streaming {

class SenderConfiguration {
  // The sender and receiver's SSRC identifiers.
  uint32_t sender_ssrc;
  uint32_t receiver_ssrc;

  // The total amount of time between a frame capture and its playback on
  // the receiver.
  std::pair<platform::Clock::duration, platform::Clock::duration>
      playout_limits;

  // The initial playout delay for animated content.
  platform::Clock::duration animated_playout_delay;

  // The type/encoding of frame data.
  RtpPayloadType payload_type;

  // If true, use an external hardware encoder.
  bool use_external_encoder;

  // RTP timebase: The number of RTP units advanced per second. For audio,
  // this is the sampling rate. For video, this is 90 kHz by convention.
  int rtp_timebase;

  // Number of channels. Must be 1 for video, for audio typically 2.
  int channels;

  // Bitrate limits and initial value. Note: for audio, only
  // bitrate_limits.second (max) is used.
  std::pair<int, int> bitrate_limits;
  int start_bitrate;

  // Maximum frame rate.
  double max_frame_rate;

  // The AES crypto key and initialization vector.
  std::string aes_key;
  std::string aes_iv_mask;

  // Code used for signal compression, and codec specific parameters. Since
  // we support arbitrary codecs through implementation of the platform APIs,
  // the embedder can choose to pass custom parameters to the encoder here.
  std::string codec;
  VideoCodecParams video_codec_params;
  std::unordered_map<std::string, std::string> custom_codec_params;
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_PLATFORM_API_SENDER_CONFIGURATION_H_
