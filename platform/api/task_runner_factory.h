// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TASK_RUNNER_FACTORY_H_
#define PLATFORM_API_TASK_RUNNER_FACTORY_H_

#include <deque>
#include <functional>
#include <list>
#include <memory>
#include <utility>

#include "platform/api/task_runner.h"

namespace openscreen {
namespace platform {

class TaskRunnerFactory {
 public:
  // Creates a instantiated, running TaskRunner
  static std::unique_ptr<TaskRunner> Create();
};
}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_TASK_RUNNER_FACTORY_H_
