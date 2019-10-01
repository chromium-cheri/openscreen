// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "util/operation_loop.h"

#include <algorithm>

#include "platform/api/logging.h"

namespace openscreen {

OperationLoop::OperationLoop(std::vector<std::function<void()>> closures)
    : closures_(closures) {
  OSP_DCHECK(closures_.size());
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
  for (std::function<void()> closure : closures_) {
    closure();
  }
}

}  // namespace openscreen
