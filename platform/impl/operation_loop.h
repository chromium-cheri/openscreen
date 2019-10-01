// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_OPERATION_LOOP_H_
#define PLATFORM_IMPL_OPERATION_LOOP_H_

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "platform/base/macros.h"

namespace openscreen {
namespace platform {

class OperationLoop {
 public:
  // An operation is a class which is expected to execute a function repeatedly.
  class Operation {
   public:
    virtual ~Operation() = default;

    // Executed the operation associated with this instance.
    virtual void operator()() = 0;
  };

  // Creates a new OperationLoop from a variable number of operations. All
  // operations are expected to live for the duration of this object's lifetime.
  OperationLoop(std::vector<Operation*> operations);

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
  const std::vector<Operation*> operations_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_OPERATION_LOOP_H_
