// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/offer.h"

namespace cast {
namespace streaming {

namespace {
  const char kAesIvMaskDeprecated[] = "aes-iv-mask";
  const char kAesKeyDeprecated[] = "aes-key";
  const char kAesIvMask[] = "aesIvMask";
  const char kAesKey[] = "aesKey";
  const char kAudioSource[] = "audio_source";
  const char kBitrate[] = "bitRate";
  const char kCastMode[] = "castMode";
  const char kChannels[] = "channels";
  const char kCastPliMessage[] = "PLI";
  const char kCodecName[] = "codecName";
  const char kErrorRecoveryMode[] = "errorRecoveryMode";
  const char kHeight[] = "height";
  const char kIncomingSsrc[] = "ssrc";
  const char kOffer[] = "offer";
  const char kReceiverGetStatus[] = "receiverGetStatus";
  const char kReceiverRtcpDscp[] = "receiverRtcpDscp";
  const char kReceiverRtcpEventLog[] = "receiverRtcpEventLog";
  const char kResolutions[] = "resolutions";
  const char kRtpExtensions[] = "rtpExtensions";
  const char kRtpPayloadType[] = "rtpPayloadType";
  const char kRtpProfile[] = "rtpProfile";
  const char kSeqNum[] = "seqNum";
  const char kStreamIndex[] = "index";
  const char kStreamType[] = "type";
  const char kSupportedStreams[] = "supportedStreams";
  const char kTargetDelay[] = "targetDelay";
  const char kTimeBase[] = "timeBase";
  const char kUserAgent[] = "userAgent";
  const char kVideoSource[] = "video_source";
  const char kWidth[] = "width";

  // Cast modes, default is "mirroring"
  const char kCastMirroring[] = "mirroring";
  const char kCastRemoting[] = "remoting";
}

}
}