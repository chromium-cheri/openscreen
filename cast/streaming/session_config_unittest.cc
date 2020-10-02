// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/session_config.h"

#include "cast/streaming/constants.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace cast {

namespace {

const SessionConfig kValidConfig{
    1223321,
    4567223,
    96000,
    5,
    kDefaultTargetPlayoutDelay,
    {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}};

bool HasInvalidParameter(const SessionConfig& config) {
  return Error::Code::kParameterInvalid == config.CheckValidity().code();
}
}  // namespace

TEST(SessionConfigTest, ValidConfig) {
  EXPECT_EQ(Error::None(), kValidConfig.CheckValidity());
}

TEST(SessionConfigTest, InvalidRtpTimebase) {
  SessionConfig config = kValidConfig;
  config.rtp_timebase = 5000;
  EXPECT_TRUE(HasInvalidParameter(config));
  config.rtp_timebase = 0;
  EXPECT_TRUE(HasInvalidParameter(config));
}

TEST(SessionConfigTest, InvalidChannelCount) {
  SessionConfig config = kValidConfig;
  config.channels = 0;
  EXPECT_TRUE(HasInvalidParameter(config));
  config.channels = -1;
  EXPECT_TRUE(HasInvalidParameter(config));
}

TEST(SessionConfigTest, InvalidPlayoutDelay) {
  SessionConfig config = kValidConfig;
  config.target_playout_delay = std::chrono::milliseconds(0);
  EXPECT_TRUE(HasInvalidParameter(config));
}

TEST(SessionConfigTest, InvalidAESKey) {
  SessionConfig config = kValidConfig;
  config.aes_secret_key = {};
  EXPECT_TRUE(HasInvalidParameter(config));
}

TEST(SessionConfigTest, InvalidAESMask) {
  SessionConfig config = kValidConfig;
  config.aes_iv_mask = {};
  EXPECT_TRUE(HasInvalidParameter(config));
}

}  // namespace cast
}  // namespace openscreen
