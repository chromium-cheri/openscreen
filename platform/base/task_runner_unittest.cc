// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <thread>

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

TEST(TaskRunnerTest, TaskRunnerExecutesTask) {
  auto runner = std::unique_ptr<TaskRunnerImpl>(new TaskRunnerImpl());

  bool task_has_run = false;
  const auto task = std::bind([&task_has_run] { task_has_run = true; });

  runner->PostTask(task);
  EXPECT_FALSE(task_has_run);

  runner->RunUntilIdleForTesting();
  EXPECT_TRUE(task_has_run);
}

TEST(TaskRunnerTest, SingleThreadedTaskRunnerRunsSequentially) {
  auto runner = std::unique_ptr<TaskRunnerImpl>(new TaskRunnerImpl());

  std::string ran_tasks;
  const auto task_one = std::bind([&ran_tasks] { ran_tasks += "1"; });

  const auto task_two = std::bind([&ran_tasks] { ran_tasks += "2"; });

  runner->PostTask(task_one);
  runner->PostTask(task_two);
  EXPECT_EQ(ran_tasks, "");

  runner->RunUntilIdleForTesting();
  EXPECT_EQ(ran_tasks, "12");
}

TEST(TaskRunnerTest, TaskRunnerCanQuitRunning) {
  auto runner = std::unique_ptr<TaskRunnerImpl>(new TaskRunnerImpl());

  std::string ran_tasks;
  const auto task_one = std::bind([&ran_tasks] { ran_tasks += "1"; });
  const auto task_two = std::bind([&ran_tasks] { ran_tasks += "2"; });

  runner->PostTask(task_one);
  EXPECT_EQ(ran_tasks, "");

  auto quit_task = runner->QuitTask();
  runner->Run();

  WaitUntilCondition(std::bind([&ran_tasks] { return !ran_tasks.empty(); }));
  EXPECT_EQ(ran_tasks, "1");

  // Since the quit task is posted first, and the single threaded task
  // runner should honor the queue, we know the task runner is not running
  // since task two doesn't get posted.
  runner->PostTask(quit_task);
  runner->PostTask(task_two);
  EXPECT_EQ(ran_tasks, "1");

  WaitUntilCondition(
      std::bind([&runner] { return runner->HasQuitForTesting(); }));
  EXPECT_EQ(ran_tasks, "1");
}

TEST(TaskRunnerTest, TaskRunnerDeletesQueuedTasksAtQuit) {
  auto runner = std::unique_ptr<TaskRunnerImpl>(new TaskRunnerImpl());

  std::string ran_tasks;
  const auto task_one = std::bind([&ran_tasks] { ran_tasks += "1"; });

  auto quit_task = runner->QuitTask();

  runner->PostTask(quit_task);
  runner->PostTask(task_one);
  runner->Run();

  runner->RunUntilIdleForTesting();

  // Because the quit task should be executed first, task_one should never run.
  EXPECT_EQ(ran_tasks, "");
}
}  // namespace platform
}  // namespace openscreen
