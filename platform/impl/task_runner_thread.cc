// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/task_runner_thread.h"

namespace openscreen {
namespace platform {

TaskRunnerThread::TaskRunnerThread(std::unique_ptr<TaskRunnerImpl> task_runner)
    : task_runner_(std::move(task_runner)) {
  thread_ = std::make_unique<std::thread>(
      [this]() { this->task_runner_->RunUntilStopped(); });
}

TaskRunnerThread::~TaskRunnerThread() {
  task_runner_->RequestStopSoon();
  thread_->join();
}

}  // namespace platform
}  // namespace openscreen
