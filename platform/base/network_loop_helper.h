// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_NETWORK_LOOP_HELPER_H_
#define PLATFORM_BASE_NETWORK_LOOP_HELPER_H_

#include <functional>
#include <memory>
#include <vector>

#include "platform/api/time.h"
#include "util/operation_loop.h"

namespace openscreen {
namespace platform {
namespace NetworkLoop {

using Clock = platform::Clock;
using OperationWithTimeout = OperationLoop::OperationWithTimeout;

// This interface is intended for use with util/operation_loop to perform
// networking operations. Each NetworkingOperation will have its
// PerformNetworkingOperations called repeatedly.
class NetworkOperation {
 public:
  virtual ~NetworkOperation() = default;

  // Executes the networking operations associated with this class. The
  // implimenting class should exit once the duration provided as timeout has
  // elapsed.
  virtual void PerformNetworkingOperations(Clock::duration timeout) = 0;
};

// This method is intended for use with util/operation_loop to call multiple
// std::unique_ptr<T> objects where each T impliments interface
// NetworkOperation. Each std::unique_ptr<T> will only have its
// PerformNetworkingOperations method called once the unique_ptr is no longer
// empty.
template <typename... T>
std::vector<OperationWithTimeout> GetOperations(
    const std::unique_ptr<T>&... args) {
  return std::vector<OperationWithTimeout>{[&args](Clock::duration timeout) {
    if (args.get()) {
      args->PerformNetworkingOperations(timeout);
    }
  }...};
}

}  // namespace NetworkLoop
}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_BASE_NETWORK_LOOP_HELPER_H_
