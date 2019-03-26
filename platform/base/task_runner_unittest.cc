// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <thread>

#include "platform/api/task_runner_factory.h"
#include "platform/api/time.h"
#include "platform/base/task_runner_impl.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {
using openscreen::platform::Clock;
using std::chrono::milliseconds;

const auto kTaskRunnerSleepTime = milliseconds(10);
const auto kTaskRunnerRunTimeout = milliseconds(500);

void WaitUntilCondition(std::function<bool()> func) {
  const auto start_time = Clock::now();
  while (((Clock::now() - start_time) < kTaskRunnerRunTimeout) && !func()) {
    std::this_thread::sleep_for(kTaskRunnerSleepTime);
  }
}
}  // anonymous namespace

namespace openscreen {
namespace platform {

TEST(TaskRunnerTest, TaskRunnerFromFactoryExecutesTask) {
  auto runner = TaskRunnerFactory::Create();

  bool task_has_run = false;
  const auto task = [&task_has_run] { task_has_run = true; };
  EXPECT_FALSE(task_has_run);

  runner->PostTask(task);

  WaitUntilCondition([&task_has_run] { return task_has_run; });
  EXPECT_TRUE(task_has_run);
}

TEST(TaskRunnerTest, SingleThreadedTaskRunnerRunsSequentially) {
  auto runner = std::unique_ptr<TaskRunnerImpl>(new TaskRunnerImpl());

  std::string ran_tasks;
  const auto task_one = [&ran_tasks] { ran_tasks += "1"; };
  const auto task_two = [&ran_tasks] { ran_tasks += "2"; };

  runner->PostTask(task_one);
  runner->PostTask(task_two);
  EXPECT_EQ(ran_tasks, "");

  runner->RunUntilIdleForTesting();
  EXPECT_EQ(ran_tasks, "12");
}

TEST(TaskRunnerTest, TaskRunnerCanStopRunning) {
  auto runner = std::unique_ptr<TaskRunnerImpl>(new TaskRunnerImpl());

  std::string ran_tasks;
  const auto task_one = [&ran_tasks] { ran_tasks += "1"; };
  const auto task_two = [&ran_tasks] { ran_tasks += "2"; };

  runner->PostTask(task_one);
  EXPECT_EQ(ran_tasks, "");

  std::thread start_thread([&runner] { runner->Start(); });

  WaitUntilCondition([&ran_tasks] { return !ran_tasks.empty(); });
  EXPECT_EQ(ran_tasks, "1");

  // Since Stop is called first, and the single threaded task
  // runner should honor the queue, we know the task runner is not running
  // since task two doesn't get ran.
  runner->Stop();
  runner->PostTask(task_two);
  EXPECT_EQ(ran_tasks, "1");

  WaitUntilCondition([&runner] { return runner->HasStoppedForTesting(); });
  EXPECT_EQ(ran_tasks, "1");

  start_thread.join();
}

TEST(TaskRunnerTest, StoppingDoesNotDeleteTasks) {
  auto runner = std::unique_ptr<TaskRunnerImpl>(new TaskRunnerImpl());

  std::string ran_tasks;
  const auto task_one = [&ran_tasks] { ran_tasks += "1"; };

  runner->PostTask(task_one);
  runner->Stop();

  EXPECT_EQ(ran_tasks, "");
  runner->RunUntilIdleForTesting();

  EXPECT_EQ(ran_tasks, "1");
}

TEST(TaskRunnerTest, TaskRunnerIsStableWithLotsOfTasks) {
  auto runner = std::unique_ptr<TaskRunnerImpl>(new TaskRunnerImpl());

  const int kNumberOfTasks = 500;
  std::string expected_ran_tasks;
  expected_ran_tasks.append(kNumberOfTasks, '1');

  std::string ran_tasks;
  for (int i = 0; i < kNumberOfTasks; ++i) {
    const auto task = [&ran_tasks] { ran_tasks += "1"; };
    runner->PostTask(task);
  }

  runner->RunUntilIdleForTesting();
  EXPECT_EQ(ran_tasks, expected_ran_tasks);
}
}  // namespace platform
}  // namespace openscreen
