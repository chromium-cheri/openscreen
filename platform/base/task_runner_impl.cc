// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "platform/base/task_runner_impl.h"

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {

std::unique_ptr<TaskRunner> CreateTaskRunner() {
  return std::unique_ptr<TaskRunner>(new TaskRunnerImpl());
}

TaskRunnerImpl::~TaskRunnerImpl() {
  Quit();
}

void TaskRunnerImpl::PostTask(Task task) {
  std::lock_guard<std::mutex> lock(task_mutex);
  tasks_.push(std::move(task));
}

void TaskRunnerImpl::PostTaskWithDelay(Task task, Clock::duration delay) {
  const Clock::time_point min_time = Clock::now() + delay;

  std::lock_guard<std::mutex> lock(delayed_task_mutex);
  delayed_tasks_.emplace_back(std::move(task), min_time);
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

absl::optional<TaskRunner::Task> TaskRunnerImpl::GetNextTask() {
  if (has_quit_) {
    return absl::nullopt;
  }

  if (tasks_.empty()) {
    return absl::nullopt;
  }

  std::lock_guard<std::mutex> lock(task_mutex);
  // Tasks could have emptied while waiting for the lock, so let's recheck.
  if (tasks_.empty()) {
    return absl::nullopt;
  }

  auto task_to_run = std::move(tasks_.front());
  tasks_.pop();

  return absl::optional<TaskRunnerImpl::Task>(task_to_run);
}

void TaskRunnerImpl::RunCurrentTasks() {
  for (auto t = GetNextTask(); t.has_value(); t = GetNextTask()) {
    t.value()();
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
      std::lock_guard<std::mutex> lock(task_mutex);
      std::lock_guard<std::mutex> delayed_lock(delayed_task_mutex);
      tasks_.push(std::move(it->first));
      it = delayed_tasks_.erase(it);
    } else {
      ++it;
    }
  }
}

void TaskRunnerImpl::Quit() {
  // boolean setting is atomic, so no lock needed.
  has_quit_ = true;

  std::unique_lock<std::mutex> lock(task_mutex);
  TaskQueue empty_queue;
  std::swap(tasks_, empty_queue);
  lock.unlock();

  std::lock_guard<std::mutex> delayed_lock(delayed_task_mutex);
  delayed_tasks_.clear();
}
}  // namespace platform
}  // namespace openscreen
