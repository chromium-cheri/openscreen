// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_RUNTIME_CONTEXT_H_
#define PLATFORM_BASE_RUNTIME_CONTEXT_H_

#include "platform/api/task_runner.h"

namespace openscreen {

class RuntimeContext {
 public:
  RuntimeContext(platform::TaskRunner* task_runner)
      : task_runner_(task_runner) {}

  platform::TaskRunner* task_runner() const { return task_runner_; }

  bool IsRunningOnTaskRunner() const {
    return task_runner_->IsRunningOnTaskRunner();
  }

 private:
  platform::TaskRunner* const task_runner_;
};

}  // namespace openscreen

#endif  // PLATFORM_BASE_RUNTIME_CONTEXT_H_
