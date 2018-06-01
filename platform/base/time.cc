// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/time.h"

#include <chrono>

namespace openscreen {
namespace platform {

Microseconds GetMonotonicTimeNow() {
  return Microseconds(std::chrono::duration_cast<std::chrono::microseconds>(
                          std::chrono::steady_clock::now().time_since_epoch())
                          .count());
}

Microseconds GetUTCNow() {
  return GetMonotonicTimeNow();
}

}  // namespace platform
}  // namespace openscreen
