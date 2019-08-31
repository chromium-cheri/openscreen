// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_TASK_RUNNER_THREAD_H_
#define PLATFORM_IMPL_TASK_RUNNER_THREAD_H_

#include <thread>

#include "platform/impl/task_runner.h"

namespace openscreen {
namespace platform {

// This is the class responsible for handling the threading associated with
// a task runner. More specifically, when this object is created, it starts a
// thread on which TaskRunnerImpl's RunUntilStopped methos is called, and upon
// destruction it calls TaskRunnerImpl's  RequestStopSoon method and joins the
// thread it created, blocking until the NetworkReader's operation completes.
class TaskRunnerThread {
 public:
  TaskRunnerThread(std::unique_ptr<TaskRunnerImpl> task_runner);
  ~TaskRunnerThread();

  TaskRunner* get() { return task_runner_.get(); }
  TaskRunner* operator->() { return task_runner_.get(); }

 private:
  std::unique_ptr<std::thread> thread_;
  std::unique_ptr<TaskRunnerImpl> task_runner_;

  OSP_DISALLOW_COPY_AND_ASSIGN(TaskRunnerThread);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_TASK_RUNNER_THREAD_H_
