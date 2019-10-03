// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_PUBLIC_CODEC_H_
#define CAST_COMMON_PUBLIC_CODEC_H_

namespace cast {
namespace api {

enum class Codec {
  kUnknown = 0,

  // Audio codecs
  kAudioOpus,
  kAudioPcm16,
  kAudioAac,
  kAudioRemote,

  // Video codecs
  kVideoFake,
  kVideoVp8,
  kVideoH264,
  kVideoRemote,
};

}  // namespace api
}  // namespace cast

#endif  // CAST_COMMON_PUBLIC_CODEC_H_
