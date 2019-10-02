// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/operation_loop.h"

#include <chrono>

#include "gtest/gtest.h"

namespace openscreen {
namespace {

TEST(OperationsLoopTest, PerformAllOperationsWaits) {
  Clock::duration kTimeout(0);
  Clock::duration kMinRuntime(500);
  std::function<void(Clock::duration)> test_function =
      [](Clock::duration timeout) {};
  OperationLoop loop({test_function}, kTimeout, kMinRuntime);

  const auto start_time = Clock::now();
  loop.PerformAllOperationsForTesting();
  EXPECT_GE(std::chrono::nanoseconds(Clock::now() - start_time).count(),
            std::chrono::nanoseconds(kMinRuntime).count());
}

}  // namespace
}  // namespace openscreen
