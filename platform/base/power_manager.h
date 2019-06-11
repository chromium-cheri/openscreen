// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_POWER_MANAGER_H_
#define PLATFORM_BASE_POWER_MANAGER_H_

#include <atomic>
#include <memory>

#include "platform/api/power_save_blocker.h"

namespace openscreen {
namespace platform {

class PowerManager {
 public:
  PowerManager() = delete;
  ~PowerManager() = delete;
  PowerManager(const PowerManager&) = delete;
  PowerManager(PowerManager&&) = delete;
  PowerManager& operator=(const PowerManager&) = delete;
  PowerManager& operator=(PowerManager&&) = delete;

  // Wake locks are OS level locks that ensure the system stays
  // awake, so we have no way of properly handling multiple
  // instances of PowerSaveBlocker.
  static void RequestWakeLock();
  static void ReleaseWakeLock();
};
}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_BASE_POWER_MANAGER_H_
