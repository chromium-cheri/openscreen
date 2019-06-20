// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/network_runner_impl.h"

#include "gtest/gtest.h"
#include "platform/base/task_runner_impl.h"
#include "platform/test/fake_clock.h"

namespace openscreen {
namespace platform {

// Mock Task Runner
class MockTaskRunner final : public TaskRunner {
 public:
  MockTaskRunner(int check_num) : check_num_(check_num) {}
  void PostPackagedTask(Task t) override {}
  void PostPackagedTaskWithDelay(Task t, Clock::duration duration) override {}
  void RunUntilStopped(bool is_async = true) override {}
  void RequestStopSoon() override {}

  int GetCheckNum() { return check_num_; }

 private:
  int check_num_;
};

class TestNetworkRunner : public NetworkRunnerImpl {
 public:
  TestNetworkRunner(std::function<TaskRunner*()> task_runner_factory)
      : NetworkRunnerImpl(task_runner_factory, NetworkLoop::Create()) {}

  TaskRunner* GetTaskRunner() { return task_runner_.get(); }
};

TEST(NetworkRunnerImplTest, TestDefaultTaskRunnerCreated) {
  auto factory = []() { return new MockTaskRunner(1); };
  TestNetworkRunner test(factory);
  test.RunUntilStopped(true);
  TaskRunner* task_runner = test.GetTaskRunner();
  EXPECT_EQ(1, static_cast<MockTaskRunner*>(task_runner)->GetCheckNum());

  test.RequestStopSoon();
}

TEST(NetworkRunnerImplTest, TestNewTaskRunnerCreated) {
  auto factory = []() { return new MockTaskRunner(1); };
  TestNetworkRunner test(factory);
  test.SetTaskRunnerFactory([]() { return new MockTaskRunner(2); });
  test.RunUntilStopped(true);
  TaskRunner* task_runner = test.GetTaskRunner();
  EXPECT_EQ(2, static_cast<MockTaskRunner*>(task_runner)->GetCheckNum());

  test.RequestStopSoon();
}

}  // namespace platform
}  // namespace openscreen
