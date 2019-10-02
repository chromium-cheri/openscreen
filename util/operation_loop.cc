// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "util/operation_loop.h"

#include <algorithm>
#include <thread>

#include "platform/api/logging.h"

namespace openscreen {

OperationLoop::OperationLoop(std::vector<OperationWithTimeout> operations,
                             Clock::duration kTimeout,
                             Clock::duration kMinLoopExecutionTime)
    : kPerformAllOperationsMinExecutionTime(kMinLoopExecutionTime),
      kOperationTimeout(kTimeout),
      operations_(operations) {
  OSP_DCHECK(operations_.size());
}

void OperationLoop::RunUntilStopped() {
  OSP_CHECK(!is_running_.exchange(true));

  while (is_running_) {
    PerformAllOperations();
  }
}

void OperationLoop::RequestStopSoon() {
  is_running_.store(false);
}

void OperationLoop::PerformAllOperations() {
  auto start_time = Clock::now();

  for (OperationWithTimeout operation : operations_) {
    operation(kOperationTimeout);
  }

  const auto duration = Clock::now() - start_time;
  const auto remaining_duration =
      kPerformAllOperationsMinExecutionTime - duration;
  if (remaining_duration > Clock::duration(0)) {
    std::this_thread::sleep_for(remaining_duration);
  }
}

void OperationLoop::PerformAllOperationsForTesting() {
  PerformAllOperations();
}

}  // namespace openscreen
