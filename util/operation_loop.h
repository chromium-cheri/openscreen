// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_OPERATION_LOOP_H_
#define UTIL_OPERATION_LOOP_H_

#include <atomic>
#include <functional>
#include <vector>

#include "platform/base/macros.h"

namespace openscreen {

class OperationLoop {
 public:
  // Creates a new OperationLoop from a variable number of closures. All
  // functions are expected to be valid the duration of this object's lifetime.
  OperationLoop(std::vector<std::function<void()>> closures);

  // Runs the PerformAllOperations function in a loop until the below
  // RequestStopSoon function is called.
  void RunUntilStopped();

  // Signals for the RunUntilStopped loop to cease running.
  void RequestStopSoon();

  OSP_DISALLOW_COPY_AND_ASSIGN(OperationLoop);

 private:
  // Performs all operations which have been provided to this instance.
  void PerformAllOperations();

  // Represents whether this instance is currently "running".
  std::atomic_bool is_running_{false};

  // Operations currently being run by this object.
  const std::vector<std::function<void()>> closures_;
};

}  // namespace openscreen

#endif  // UTIL_OPERATION_LOOP_H_
