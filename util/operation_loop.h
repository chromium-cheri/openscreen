// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_OPERATION_LOOP_H_
#define UTIL_OPERATION_LOOP_H_

#include <atomic>
#include <condition_variable>
#include <functional>
#include <vector>

#include "platform/api/time.h"
#include "platform/base/macros.h"

namespace openscreen {

using Clock = platform::Clock;

class OperationLoop {
 public:
  using OperationWithTimeout = std::function<void(Clock::duration)>;

  // Creates a new OperationLoop from a variable number of operations.
  // operations = Functions to execute repeatedly. All functions are expected to
  //              be valid the duration of this object's lifetime.
  // timeout = Timeout for each individual function above.
  // min_loop_execution_time = Minimum time that OperationLoop should wait
  //                           before successive calls to members of the
  //                           provided operations vector.
  OperationLoop(std::vector<OperationWithTimeout> operations,
                Clock::duration timeout,
                Clock::duration min_loop_execution_time);

  // Runs the PerformAllOperations function in a loop until the below
  // RequestStopSoon function is called.
  void RunUntilStopped();

  // Signals for the RunUntilStopped loop to cease running.
  void RequestStopSoon();

  OSP_DISALLOW_COPY_AND_ASSIGN(OperationLoop);

 private:
  // Performs all operations which have been provided to this instance.
  void PerformAllOperations();

  const Clock::duration perform_all_operations_min_execution_time_;

  const Clock::duration operation_timeout_;

  // Used to wait in PerformAllOperations() if not enough time has elapsed.
  std::condition_variable perform_all_operations_waiter_;

  // Represents whether this instance is currently "running".
  std::atomic_bool is_running_{false};

  // Operations currently being run by this object.
  const std::vector<OperationWithTimeout> operations_;
};

}  // namespace openscreen

#endif  // UTIL_OPERATION_LOOP_H_
