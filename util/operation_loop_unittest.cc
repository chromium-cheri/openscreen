// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/operation_loop.h"

#include <chrono>

#include "gtest/gtest.h"

namespace openscreen {

class OperationLoopForTesting : public OperationLoop {
 public:
  OperationLoopForTesting(std::vector<OperationWithTimeout> operations,
                          Clock::duration timeout,
                          Clock::duration min_loop_execution_time)
      : OperationLoop(operations, timeout, min_loop_execution_time) {
    is_running_.store(true);
  }

  using OperationLoop::PerformAllOperations;
};

TEST(OperationsLoopTest, PerformAllOperationsWaits) {
  constexpr Clock::duration kTimeout{0};
  constexpr Clock::duration kMinRuntime{500};
  std::function<void(Clock::duration)> test_function =
      [](Clock::duration timeout) {};
  OperationLoopForTesting loop({test_function}, kTimeout, kMinRuntime);

  const auto start_time = Clock::now();
  loop.PerformAllOperations();
  EXPECT_GE(std::chrono::nanoseconds(Clock::now() - start_time).count(),
            std::chrono::nanoseconds(kMinRuntime).count());
}

}  // namespace openscreen
