// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/public/cast_streaming_app_ids.h"

#include <algorithm>

#include "gtest/gtest.h"

namespace openscreen {
namespace cast {

TEST(CastStreamingAppIdsTest, Test) {
  EXPECT_TRUE(IsCastStreamingAppId("0F5096E8"));
  EXPECT_TRUE(IsCastStreamingAppId("85CDB22F"));
  EXPECT_FALSE(IsCastStreamingAppId("DEADBEEF"));

  EXPECT_TRUE(IsCastStreamingAudioVideoAppId("0F5096E8"));
  EXPECT_FALSE(IsCastStreamingAudioVideoAppId("85CDB22F"));
  EXPECT_FALSE(IsCastStreamingAudioVideoAppId("DEADBEEF"));

  EXPECT_FALSE(IsCastStreamingAudioOnlyAppId("0F5096E8"));
  EXPECT_TRUE(IsCastStreamingAudioOnlyAppId("85CDB22F"));
  EXPECT_FALSE(IsCastStreamingAudioOnlyAppId("DEADBEEF"));

  auto app_ids = GetCastStreamingAppIds();
  EXPECT_EQ(static_cast<size_t>(6), app_ids.size());
  EXPECT_TRUE(std::find(app_ids.begin(), app_ids.end(),
                        kChromeMirroringAppId) != app_ids.end());
  EXPECT_TRUE(std::find(app_ids.begin(), app_ids.end(),
                        kChromeAudioMirroringAppId) != app_ids.end());
  EXPECT_TRUE(std::find(app_ids.begin(), app_ids.end(),
                        kAndroidMirroringAppId) != app_ids.end());
  EXPECT_TRUE(std::find(app_ids.begin(), app_ids.end(),
                        kAndroidAudioMirroringAppId) != app_ids.end());
  EXPECT_TRUE(std::find(app_ids.begin(), app_ids.end(),
                        kAndroidAppMirroringAppId) != app_ids.end());
  EXPECT_TRUE(std::find(app_ids.begin(), app_ids.end(),
                        kIosAppMirroringAppId) != app_ids.end());

  auto av_app_ids = GetCastStreamingAudioVideoAppIds();
  EXPECT_EQ(static_cast<size_t>(4), av_app_ids.size());
  EXPECT_TRUE(std::find(av_app_ids.begin(), av_app_ids.end(),
                        kChromeMirroringAppId) != av_app_ids.end());
  EXPECT_TRUE(std::find(av_app_ids.begin(), av_app_ids.end(),
                        kAndroidMirroringAppId) != av_app_ids.end());
  EXPECT_TRUE(std::find(av_app_ids.begin(), av_app_ids.end(),
                        kAndroidAppMirroringAppId) != av_app_ids.end());
  EXPECT_TRUE(std::find(av_app_ids.begin(), av_app_ids.end(),
                        kIosAppMirroringAppId) != av_app_ids.end());

  auto audio_app_ids = GetCastStreamingAudioOnlyAppIds();
  EXPECT_EQ(static_cast<size_t>(2), audio_app_ids.size());
  EXPECT_TRUE(std::find(audio_app_ids.begin(), audio_app_ids.end(),
                        kChromeAudioMirroringAppId) != audio_app_ids.end());
  EXPECT_TRUE(std::find(audio_app_ids.begin(), audio_app_ids.end(),
                        kAndroidAudioMirroringAppId) != audio_app_ids.end());
}

}  // namespace cast
}  // namespace openscreen
