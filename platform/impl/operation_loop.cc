// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "platform/impl/operation_loop.h"

#include <algorithm>

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {

OperationLoop::OperationLoop(std::vector<Operation*> operations)
    : operations_(operations) {
  OSP_DCHECK(operations_.size());
}

void OperationLoop::RunUntilStopped() {
  const bool was_running = is_running_.exchange(true);
  OSP_CHECK(!was_running);

  while (is_running_) {
    PerformAllOperations();
  }
}

void OperationLoop::RequestStopSoon() {
  is_running_.store(false);
}

void OperationLoop::PerformAllOperations() {
  for (Operation* operation : operations_) {
    (*operation)();
  }
}

}  // namespace platform
}  // namespace openscreen
