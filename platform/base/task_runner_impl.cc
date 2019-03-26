// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "platform/base/task_runner_impl.h"

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {
TaskRunnerImpl::TaskRunnerImpl(platform::ClockNowFunctionPtr now_function)
    : now_function_(now_function) {}

TaskRunnerImpl::~TaskRunnerImpl() {
  Stop();
}

void TaskRunnerImpl::PostTask(Task task) {
  std::lock_guard<std::mutex> lock(task_mutex_);
  tasks_.push_back(std::move(task));
  run_loop_wakeup_.notify_all();
}

void TaskRunnerImpl::PostTaskWithDelay(Task task, Clock::duration delay) {
  std::unique_lock<std::mutex> delayed_lock(delayed_task_mutex_);
  delayed_tasks_.emplace_back(std::move(task), now_function_() + delay);
  delayed_lock.unlock();

  std::lock_guard<std::mutex> lock(task_mutex_);
  run_loop_wakeup_.notify_all();
}

void TaskRunnerImpl::Start() {
  has_stopped_ = false;
  RunTasksUntilStopped();
}

void TaskRunnerImpl::Stop() {
  has_stopped_ = true;
}

void TaskRunnerImpl::RunUntilIdleForTesting() {
  ScheduleDelayedTasks();
  RunCurrentTasks();
}

std::deque<TaskRunner::Task>
TaskRunnerImpl::PopDelayedTasksReadyForScheduling() {
  std::deque<Task> tasks_to_enqueue;
  std::unique_lock<std::mutex> delayed_lock(delayed_task_mutex_);

  auto it = delayed_tasks_.begin();
  while (it != delayed_tasks_.end()) {
    if (now_function_() >= it->second) {
      tasks_to_enqueue.push_back(std::move(it->first));
      it = delayed_tasks_.erase(it);
    } else {
      ++it;
    }
  }

  return tasks_to_enqueue;
}

bool TaskRunnerImpl::RunCurrentTasks() {
  std::deque<Task> current_tasks;

  std::lock_guard<std::mutex> lock(task_mutex_);
  tasks_.swap(current_tasks);
  task_mutex_.unlock();

  for (auto task : current_tasks) {
    task();
  }

  return !current_tasks.empty();
}

void TaskRunnerImpl::RunTasksUntilStopped() {
  while (!has_stopped_) {
    ScheduleDelayedTasks();

    if (!RunCurrentTasks()) {
      std::unique_lock<std::mutex> lock(task_mutex_);
      run_loop_wakeup_.wait(lock);
    }
  }
}

void TaskRunnerImpl::ScheduleDelayedTasks() {
  auto tasks_to_enqueue = PopDelayedTasksReadyForScheduling();

  std::lock_guard<std::mutex> lock(task_mutex_);
  tasks_.insert(tasks_.end(), tasks_to_enqueue.begin(), tasks_to_enqueue.end());
}
}  // namespace platform
}  // namespace openscreen
