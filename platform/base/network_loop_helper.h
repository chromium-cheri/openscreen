// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_NETWORK_LOOP_HELPER_H_
#define PLATFORM_BASE_NETWORK_LOOP_HELPER_H_

#include <functional>
#include <memory>
#include <vector>

#include "platform/api/time.h"

namespace openscreen {
namespace NetworkLoop {

using Clock = platform::Clock;

class NetworkOperation {
 public:
  virtual ~NetworkOperation() = default;

  // Executes the networking operations associated with this class.
  virtual void PerformNetworkingOperations(Clock::duration timeout) = 0;
};

template <typename... T>
std::vector<std::function<void(Clock::duration)>> GetOperations(
    const std::unique_ptr<T>&... args) {
  return {[&args](Clock::duration timeout) {
    return WrapOptionalNetworkOperation(args, timeout);
  }...};
}

template <typename T>
void WrapOptionalNetworkOperation(const std::unique_ptr<T>& operation,
                                  Clock::duration timeout) {
  if (operation.get()) {
    operation->PerformNetworkingOperations(timeout);
  }
}

}  // namespace NetworkLoop
}  // namespace openscreen

#endif  // PLATFORM_BASE_NETWORK_LOOP_HELPER_H_
