// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "api/public/task_runner_factory.h"

#include <thread>

#include "api/impl/task_runner_impl.h"
#include "platform/api/logging.h"

namespace openscreen {
namespace platform {

// static
std::unique_ptr<TaskRunner> TaskRunnerFactory::Create() {
  auto task_runner =
      std::unique_ptr<TaskRunner>(new TaskRunnerImpl(platform::Clock::now));

  std::thread task_runner_thread([&task_runner] {
    static_cast<TaskRunnerImpl*>(task_runner.get())->Start();
  });
  task_runner_thread.detach();

  return task_runner;
}

}  // namespace platform
}  // namespace openscreen
