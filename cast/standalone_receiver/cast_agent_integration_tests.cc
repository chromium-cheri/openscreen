// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast_agent.h"
#include "gtest/gtest.h"
#include "platform/impl/network_interface.h"
#include "platform/impl/platform_client_posix.h"
#include "platform/impl/task_runner.h"

namespace openscreen {
namespace cast {
namespace {

class CastAgentIntegrationTest : public ::testing::Test {
 public:
  void SetUp() override {
    PlatformClientPosix::Create(Clock::duration{50}, Clock::duration{50},
                                std::make_unique<TaskRunnerImpl>(&Clock::now));
  }

  void TearDown() override { PlatformClientPosix::ShutDown(); }
};

TEST_F(CastAgentIntegrationTest, CanStartAndStop) {
  auto platform_client = PlatformClientPosix::GetInstance();

  absl::optional<InterfaceInfo> loopback = GetLoopbackInterfaceForTesting();
  ASSERT_TRUE(loopback.has_value());

  CastAgent agent(platform_client->GetTaskRunner(), loopback.value());
  EXPECT_TRUE(agent.Start().ok());
  EXPECT_TRUE(agent.Stop().ok());
}

}  // namespace
}  // namespace cast
}  // namespace openscreen
