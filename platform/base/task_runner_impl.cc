// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "platform/base/task_runner_impl.h"

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {
TaskRunnerImpl::~TaskRunnerImpl() {
  Quit();
}

void TaskRunnerImpl::CancelTask(TaskId id) {
  for (auto it = tasks_.begin(); it != tasks_.end(); ++it) {
    if (it->first == id) {
      tasks_.erase(it);
      return;
    }
  }

  for (auto it = delayed_tasks_.begin(); it != delayed_tasks_.end(); ++it) {
    if (it->first.first == id) {
      delayed_tasks_.erase(it);
      return;
    }
  }
}

TaskId TaskRunnerImpl::PostTask(Task task) {
  std::lock_guard<std::mutex> lock(task_mutex_);

  auto task_id = GetNextTaskId();
  tasks_.push_back(std::make_pair(task_id, std::move(task)));

  return task_id;
}

TaskId TaskRunnerImpl::PostTaskWithDelay(Task task, Clock::duration delay) {
  std::lock_guard<std::mutex> lock(delayed_task_mutex_);

  auto task_id = GetNextTaskId();
  ScheduledTask scheduled_task = std::make_pair(task_id, std::move(task));
  delayed_tasks_.emplace_back(scheduled_task, Clock::now() + delay);

  return task_id;
}

TaskRunnerImpl::Task TaskRunnerImpl::QuitTask() {
  return std::bind([this] { this->Quit(); });
}

void TaskRunnerImpl::Run() {
  has_quit_ = false;
  auto callable = std::bind([this] { this->RunLoop(); });

  // Local creation and detaching of the thread here keeps us from
  // having to join or delete it later.
  std::thread(callable).detach();
}

void TaskRunnerImpl::RunUntilIdleForTesting() {
  while (!has_quit_ && (!tasks_.empty() || !delayed_tasks_.empty())) {
    ScheduleDelayedTasks();
    RunCurrentTasks();
  }
}

absl::optional<TaskRunnerImpl::ScheduledTask> TaskRunnerImpl::GetNextTask() {
  if (has_quit_) {
    return absl::nullopt;
  }

  std::lock_guard<std::mutex> lock(task_mutex_);
  if (tasks_.empty()) {
    return absl::nullopt;
  }

  ScheduledTask task_to_run = std::move(tasks_.front());
  tasks_.pop_front();

  return absl::optional<ScheduledTask>(task_to_run);
}

void TaskRunnerImpl::RunCurrentTasks() {
  for (auto t = GetNextTask(); t.has_value(); t = GetNextTask()) {
    t.value().second();
  }
}

void TaskRunnerImpl::RunLoop() {
  while (!has_quit_) {
    ScheduleDelayedTasks();
    RunCurrentTasks();
  }
}

void TaskRunnerImpl::ScheduleDelayedTasks() {
  const Clock::time_point now = Clock::now();

  auto it = delayed_tasks_.begin();
  while (it != delayed_tasks_.end()) {
    if (now >= it->second) {
      std::lock_guard<std::mutex> lock(task_mutex_);
      std::lock_guard<std::mutex> delayed_lock(delayed_task_mutex_);
      tasks_.push_back(std::move(it->first));
      it = delayed_tasks_.erase(it);
    } else {
      ++it;
    }
  }
}

void TaskRunnerImpl::Quit() {
  has_quit_ = true;

  std::unique_lock<std::mutex> lock(task_mutex_);
  TaskQueue empty_queue;
  std::swap(tasks_, empty_queue);
  lock.unlock();

  std::lock_guard<std::mutex> delayed_lock(delayed_task_mutex_);
  delayed_tasks_.clear();
}

std::unique_ptr<TaskRunner> CreateTaskRunner() {
  return std::unique_ptr<TaskRunner>(new TaskRunnerImpl());
}
}  // namespace platform
}  // namespace openscreen
