// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_OFFER_H_
#define CAST_STREAMING_OFFER_H_

#include "absl/types/optional.h"
#include "cast/streaming/session_config.h"
#include "platform/base/error.h"

namespace Json {
class Value;
}

namespace cast {
namespace streaming {

struct Offer {
 public:
  enum CastMode { kRemoting, kMirroring };

  static openscreen::ErrorOr<Offer> Parse(Json::Value root);

  CastMode cast_mode() const { return cast_mode_; }
  const std::vector<SessionConfig>& configs() const { return configs_; }

 private:
  Offer(CastMode cast_mode, std::vector<SessionConfig> configs);

  CastMode cast_mode_;
  absl::optional<SessionConfig> audio_config_;
  absl::optional<SessionConfig> video_config_;
};

}  // namespace streaming
}  // namespace cast

#endif
