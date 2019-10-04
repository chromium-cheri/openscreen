// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_NETWORK_LOOP_H_
#define PLATFORM_BASE_NETWORK_LOOP_H_

#include <functional>
#include <memory>
#include <vector>

#include "platform/api/time.h"
#include "util/operation_loop.h"

namespace openscreen {
namespace platform {
namespace NetworkLoop {

using Clock = platform::Clock;

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

}  // namespace NetworkLoop
}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_BASE_NETWORK_LOOP_H_
