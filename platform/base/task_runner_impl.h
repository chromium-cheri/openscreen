// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_TASK_RUNNER_IMPL_H_
#define PLATFORM_BASE_TASK_RUNNER_IMPL_H_

#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>

#include "platform/api/task_runner.h"
#include "platform/api/time.h"
#include "third_party/abseil/src/absl/types/optional.h"

namespace openscreen {
namespace platform {

class TaskRunnerImpl : public TaskRunner {
 public:
  ~TaskRunnerImpl() override;

  // TODO(jophba): change PostTask* methods to return an id, that
  // can be later used to cancel tasks.

  // Takes a Task that should be run at the first convenient time.
  void PostTask(Task task) override;

  // Takes a Task that should be run no sooner than "delay" time from now. Note
  // that we do not guarantee it will run precisely "delay" later, merely that
  // it will run no sooner than "delay" time from now
  void PostTaskWithDelay(Task task, Clock::duration delay) override;

  // A task callers can use to exit the run loop once started.
  Task QuitTask();

  // Start executing tasks
  void Run();

  // Execute all tasks immediately, useful for testing only.
  void RunUntilIdleForTesting();

  // Testing only: has the TaskRunner quit?
  bool HasQuitForTesting() { return has_quit_; }

 private:
  using DelayedTaskList = std::list<std::pair<Task, Clock::time_point>>;
  using TaskQueue = std::queue<Task>;

  // Atomically get the next task to run
  absl::optional<Task> GetNextTask();

  // Run all tasks already in the task queue
  void RunCurrentTasks();

  // Forever loop method for indepdendent thread execution
  void RunLoop();

  // Look at all tasks in the delayed task queue, then schedule them if the
  // minimum delay time has elapsed
  void ScheduleDelayedTasks();

  // Quit running and tear down the thread.
  void Quit();

  bool has_quit_;
  std::mutex task_mutex;
  std::mutex delayed_task_mutex;

  DelayedTaskList delayed_tasks_;
  TaskQueue tasks_;
};
}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_BASE_TASK_RUNNER_IMPL_H_
