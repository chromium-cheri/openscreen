// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/public/cast_streaming_app_ids.h"

#include "util/std_util.h"

namespace openscreen {
namespace cast {

bool IsCastStreamingAppId(const std::string& app_id) {
  return IsCastStreamingAudioOnlyAppId(app_id) ||
         IsCastStreamingAudioVideoAppId(app_id);
}

bool IsCastStreamingAudioVideoAppId(const std::string& app_id) {
  return Contains(GetCastStreamingAudioVideoAppIds(), app_id);
}

bool IsCastStreamingAudioOnlyAppId(const std::string& app_id) {
  return Contains(GetCastStreamingAudioOnlyAppIds(), app_id);
}

}  // namespace cast
}  // namespace openscreen
