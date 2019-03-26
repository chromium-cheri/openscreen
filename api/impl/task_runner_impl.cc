// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "api/impl/task_runner_impl.h"

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {
TaskRunnerImpl::TaskRunnerImpl(platform::ClockNowFunctionPtr now_function)
    : now_function_(now_function), is_running_(false) {}

TaskRunnerImpl::~TaskRunnerImpl() {
  Stop();
}

void TaskRunnerImpl::PostTask(Task task) {
  std::lock_guard<std::mutex> lock(task_mutex_);
  tasks_.push_back(std::move(task));
  run_loop_wakeup_.notify_all();
}

void TaskRunnerImpl::PostTaskWithDelay(Task task, Clock::duration delay) {
  std::lock_guard<std::mutex> lock(task_mutex_);
  delayed_tasks_.emplace(std::move(task), now_function_() + delay);
  run_loop_wakeup_.notify_all();
}

void TaskRunnerImpl::Start() {
  is_running_ = true;
  run_loop_wakeup_.notify_all();
  RunTasksUntilStopped();
}

void TaskRunnerImpl::Stop() {
  is_running_ = false;
  run_loop_wakeup_.notify_all();
}

void TaskRunnerImpl::RunUntilIdleForTesting() {
  ScheduleDelayedTasks();
  RunCurrentTasks();
}

bool TaskRunnerImpl::RunCurrentTasks() {
  std::deque<Task> current_tasks;

  std::lock_guard<std::mutex> lock(task_mutex_);
  tasks_.swap(current_tasks);
  task_mutex_.unlock();

  for (Task& task : current_tasks) {
    task();
  }

  return !current_tasks.empty();
}

void TaskRunnerImpl::RunTasksUntilStopped() {
  while (is_running_) {
    ScheduleDelayedTasks();

    if (!RunCurrentTasks()) {
      std::unique_lock<std::mutex> lock(task_mutex_);

      if (!delayed_tasks_.empty()) {
        run_loop_wakeup_.wait_until(lock,
                                    delayed_tasks_.top().time_runnable_after);
      } else {
        run_loop_wakeup_.wait(lock);
      }
    }
  }
}

void TaskRunnerImpl::ScheduleDelayedTasks() {
  std::lock_guard<std::mutex> lock(task_mutex_);

  while (!delayed_tasks_.empty() &&
         (delayed_tasks_.top().time_runnable_after < now_function_())) {
    tasks_.push_back(std::move(delayed_tasks_.top().task));
    delayed_tasks_.pop();
  }
}
}  // namespace platform
}  // namespace openscreen
