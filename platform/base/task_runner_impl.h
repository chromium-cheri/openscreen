// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_TASK_RUNNER_IMPL_H_
#define PLATFORM_BASE_TASK_RUNNER_IMPL_H_

#include <atomic>
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

struct TaskId {
  int task_id;

  bool operator==(const TaskId& other) { return task_id == other.task_id; }
};

class TaskRunnerImpl : public TaskRunner {
 public:
  ~TaskRunnerImpl() override;

  // TaskRunner overrides
  void CancelTask(TaskId id) override;
  TaskId PostTask(Task task) override;
  TaskId PostTaskWithDelay(Task task, Clock::duration delay) override;

  // A task callers can use to exit the run loop once started.
  Task QuitTask();

  // Start executing tasks
  void Run();

  // Execute all tasks immediately, useful for testing only.
  void RunUntilIdleForTesting();

  // Testing only: has the TaskRunner quit?
  bool HasQuitForTesting() { return has_quit_; }

 private:
  using ScheduledTask = std::pair<TaskId, Task>;

  using DelayedTaskList =
      std::list<std::pair<ScheduledTask, Clock::time_point>>;
  using TaskQueue = std::list<ScheduledTask>;

  // Atomically get the next task to run
  absl::optional<ScheduledTask> GetNextTask();
  TaskId GetNextTaskId() { return TaskId{next_task_id_++}; }

  // Run all tasks already in the task queue
  void RunCurrentTasks();

  // Forever loop method for indepdendent thread execution
  void RunLoop();

  // Look at all tasks in the delayed task queue, then schedule them if the
  // minimum delay time has elapsed
  void ScheduleDelayedTasks();

  // Quit running and tear down the thread.
  void Quit();

  std::atomic_int next_task_id_;
  std::atomic_bool has_quit_;
  std::mutex task_mutex_;
  std::mutex delayed_task_mutex_;

  DelayedTaskList delayed_tasks_;
  TaskQueue tasks_;
};
}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_BASE_TASK_RUNNER_IMPL_H_
