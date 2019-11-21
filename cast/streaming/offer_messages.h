// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_OFFER_H_
#define CAST_STREAMING_OFFER_H_

#include <vector>

#include "absl/types/optional.h"
#include "cast/streaming/session_config.h"
#include "json/value.h"
#include "platform/base/error.h"

// This file contains the implementation of the Cast V2 Mirroring Control
// Protocol offer object definition.
namespace cast {
namespace streaming {

// The *Stream classes are intended as very lightweight wrappers around
// the raw, untyped Json::Value objects provided by parsing the cast
// messages. Callers have a responsibility to ensure that excess copies
// are not created by repeatedly calling the same method.
class Stream {
 public:
  Stream(Json::Value stream);
  ~Stream();

  int index() const { return stream_["index"].asInt(); }
  std::string type() const { return stream_["type"].asString(); }
  std::string codec_name() const { return stream_["codecName"].asString(); }
  std::string rtp_profile() const { return stream_["rtpProfile"].asString(); }
  int rtp_payload_type() const { return stream_["rtpPayloadType"].asInt(); }
  Ssrc ssrc() const { return stream_["ssrc"].asUInt(); }
  int target_delay() const { return stream_["targetDelay"].asInt(); }
  std::string aes_key() const { return stream_["aesKey"].asString(); }
  std::string aes_iv_mask() const { return stream_["aesIvMask"].asString(); }
  std::string receiver_rtcp_event_log() const {
    return stream_["receiverRtcpEventLog"].asString();
  }
  std::string receiver_rtcp_dscp() const {
    return stream_["receiverRtcpDscp"].asString();
  }
  int time_base() const;

 protected:
  Json::Value stream_;
};

class AudioStream : public Stream {
 public:
  AudioStream(Json::Value stream);
  int bit_rate() const { return stream_["bitRate"].asInt(); }
  int channels() const { return stream_["channels"].asInt(); }
};

class Resolution {
 public:
  Resolution(Json::Value resolution);
  int width() const { return resolution_["width"].asInt(); }
  int height() const { return resolution_["height"].asInt(); }

 private:
  Json::Value resolution_;
};

class ResolutionList {
 public:
  ResolutionList(Json::Value resolution_list);

  int size() const { return resolution_list_.size(); }
  Resolution operator[](int index) const {
    return Resolution(resolution_list_[index]);
  }

 private:
  Json::Value resolution_list_;
};

class VideoStream : public Stream {
 public:
  VideoStream(Json::Value stream);

  std::string max_frame_rate() const {
    return stream_["maxFrameRate"].asString();
  }
  int max_bit_rate() const { return stream_["maxBitRate"].asInt(); }
  std::string protection() const { return stream_["protection"].asString(); }
  std::string profile() const { return stream_["profile"].asString(); }
  std::string level() const { return stream_["level"].asString(); }

  ResolutionList resolutions() const {
    return ResolutionList(stream_["resolutions"]);
  }

  std::string error_recovery_mode() const {
    return stream_["errorRecoveryMode"].asString();
  }
};

class Offer {
 public:
  enum CastMode { kRemoting, kMirroring };

  static openscreen::ErrorOr<Offer> Parse(Json::Value root);

  CastMode cast_mode() const { return cast_mode_; }
  const std::vector<AudioStream>& audio_streams() const {
    return audio_streams_;
  }
  const std::vector<VideoStream>& video_streams() const {
    return video_streams_;
  }

 private:
  Offer(CastMode cast_mode,
        std::vector<AudioStream> audio_streams,
        std::vector<VideoStream> video_streams);

  CastMode cast_mode_;
  std::vector<AudioStream> audio_streams_;
  std::vector<VideoStream> video_streams_;
};

}  // namespace streaming
}  // namespace cast

#endif  // CAST_STREAMING_OFFER_H_
