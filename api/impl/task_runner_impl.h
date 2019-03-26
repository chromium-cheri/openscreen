// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_TASK_RUNNER_IMPL_H_
#define API_IMPL_TASK_RUNNER_IMPL_H_

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/types/optional.h"
#include "api/public/task_runner.h"
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

  // Execute all tasks immediately, useful for testing only. Note: this method
  // will schedule any delayed tasks that are ready to run, but does not block
  // waiting for delayed tasks to become eligible.
  void RunUntilIdleForTesting();

 private:
  struct DelayedTask {
    DelayedTask(Task task_, Clock::time_point time_runnable_after_)
        : task(task_), time_runnable_after(time_runnable_after_) {}

    Task task;
    Clock::time_point time_runnable_after;

    bool operator>(const DelayedTask& other) const {
      return this->time_runnable_after > other.time_runnable_after;
    }
  };

  // Run all tasks already in the task queue. Returns whether or not any
  // tasks were actually ran.
  std::size_t RunCurrentTasks();

  // Loop method that runs tasks in the current thread, until the Stop
  // method is called.
  void RunTasksUntilStopped();

  // Look at all tasks in the delayed task queue, then schedule them if the
  // minimum delay time has elapsed.
  void ScheduleDelayedTasks();

  // Puts the task running thread into a waiting state for a notify on the
  // run loop wakeup condition variable.
  void WaitForRunLoopWakeup();

  platform::ClockNowFunctionPtr now_function_;

  bool is_running_;

  // This mutex is used for both tasks_ and notifying the run loop to wake
  // up when it is waiting for a task to be added to the queue in
  // run_loop_wakeup_.
  std::mutex task_mutex_;
  std::priority_queue<DelayedTask,
                      std::vector<DelayedTask>,
                      std::greater<DelayedTask>>
      delayed_tasks_ GUARDED_BY(task_mutex_);

  std::condition_variable run_loop_wakeup_;
  std::deque<Task> tasks_ GUARDED_BY(task_mutex_);
};
}  // namespace platform
}  // namespace openscreen

#endif  // API_IMPL_TASK_RUNNER_IMPL_H_
