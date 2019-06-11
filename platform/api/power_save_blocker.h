// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_POWER_SAVE_BLOCKER_H_
#define PLATFORM_API_POWER_SAVE_BLOCKER_H_

#include <atomic>
#include <memory>

namespace openscreen {
namespace platform {

// Ensure that the device does not got to sleep. The wait lock
// is automatically disabled as part of class destruction.
class PowerSaveBlocker {
 public:
  PowerSaveBlocker();
  virtual ~PowerSaveBlocker();
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_POWER_SAVE_BLOCKER_H_
