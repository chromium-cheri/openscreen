// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_TASK_RUNNER_IMPL_H_
#define PLATFORM_BASE_TASK_RUNNER_IMPL_H_

#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/types/optional.h"
#include "platform/api/task_runner.h"
#include "platform/api/time.h"

namespace openscreen {
namespace platform {

class TaskRunnerImpl : public TaskRunner {
 public:
  TaskRunnerImpl(platform::ClockNowFunctionPtr now_function);

  // TaskRunner overrides
  ~TaskRunnerImpl() override;
  void PostTask(Task task) override;
  void PostTaskWithDelay(Task task, Clock::duration delay) override;

  // Tasks will only be executed if Start has been called, and Stop has not.
  // Important note: TaskRunnerImpl does NOT do any threading, so calling
  // "Start()" will block whatever thread you are calling it on.
  void Start();
  void Stop();

  // Execute all tasks immediately, useful for testing only.
  void RunUntilIdleForTesting();

  // Testing only: has the TaskRunner Stop?
  bool HasStoppedForTesting() { return has_stopped_; }

 private:
  // Returns atomically whether tasks are enqueued in the standard or
  // delayed queues.
  bool AreTasksEnqueued();

  // Run all tasks already in the task queue
  void RunCurrentTasks();

  // Loop method that runs tasks in the current thread, until the Stop
  // method is called.
  void RunTasksUntilQuit();

  // Look at all tasks in the delayed task queue, then schedule them if the
  // minimum delay time has elapsed
  void ScheduleDelayedTasks();

  platform::ClockNowFunctionPtr now_function_;

  // This is atomic as it may be set from a different thread.
  std::atomic_bool has_stopped_;

  std::mutex delayed_task_mutex_;
  std::deque<std::pair<Task, Clock::time_point>> delayed_tasks_
      GUARDED_BY(delayed_task_mutex_);

  std::mutex task_mutex_;
  std::deque<Task> tasks_ GUARDED_BY(task_mutex_);
};
}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_BASE_TASK_RUNNER_IMPL_H_
