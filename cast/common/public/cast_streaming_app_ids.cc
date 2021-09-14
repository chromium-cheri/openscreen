// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/public/cast_streaming_app_ids.h"

namespace openscreen {
namespace cast {

std::vector<std::string> GetCastStreamingAppIds() {
  return std::vector<std::string>(
      {GetCastStreamingAudioVideoAppId(), GetCastStreamingAudioOnlyAppId()});
}

}  // namespace cast
}  // namespace openscreen
