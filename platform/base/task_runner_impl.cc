// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "platform/base/task_runner_impl.h"

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {
TaskRunnerImpl::~TaskRunnerImpl() {
  Stop();
}

void TaskRunnerImpl::PostTask(Task task) {
  std::lock_guard<std::mutex> lock(task_mutex_);
  tasks_.push_back(std::move(task));
}

void TaskRunnerImpl::PostTaskWithDelay(Task task, Clock::duration delay) {
  std::lock_guard<std::mutex> lock(delayed_task_mutex_);
  delayed_tasks_.emplace_back(std::move(task), Clock::now() + delay);
}

void TaskRunnerImpl::Start() {
  has_stopped_ = false;
  RunTasksUntilQuit();
}

void TaskRunnerImpl::Stop() {
  has_stopped_ = true;
}

void TaskRunnerImpl::RunUntilIdleForTesting() {
  while (AreTasksEnqueued()) {
    ScheduleDelayedTasks();
    RunCurrentTasks();
  }
}

bool TaskRunnerImpl::AreTasksEnqueued() {
  std::unique_lock<std::mutex> delayed_lock(delayed_task_mutex_);
  if (!delayed_tasks_.empty()) {
    return true;
  }
  delayed_lock.unlock();

  std::lock_guard<std::mutex> lock(task_mutex_);
  return !tasks_.empty();
}

void TaskRunnerImpl::RunCurrentTasks() {
  std::deque<Task> current_tasks;

  std::lock_guard<std::mutex> lock(task_mutex_);
  tasks_.swap(current_tasks);
  task_mutex_.unlock();

  for (auto task : current_tasks) {
    task();
  }
}

void TaskRunnerImpl::RunTasksUntilQuit() {
  while (!has_stopped_) {
    ScheduleDelayedTasks();
    RunCurrentTasks();
  }
}

void TaskRunnerImpl::ScheduleDelayedTasks() {
  const Clock::time_point now = Clock::now();

  std::deque<Task> tasks_to_enqueue;
  std::unique_lock<std::mutex> delayed_lock(delayed_task_mutex_);
  auto it = delayed_tasks_.begin();
  while (it != delayed_tasks_.end()) {
    if (now >= it->second) {
      tasks_to_enqueue.push_back(std::move(it->first));
      it = delayed_tasks_.erase(it);
    } else {
      ++it;
    }
  }
  delayed_lock.unlock();

  std::lock_guard<std::mutex> lock(task_mutex_);
  tasks_.insert(tasks_.end(), tasks_to_enqueue.begin(), tasks_to_enqueue.end());
}

std::unique_ptr<TaskRunner> CreateTaskRunner() {
  return std::unique_ptr<TaskRunner>(new TaskRunnerImpl());
}
}  // namespace platform
}  // namespace openscreen
