// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_POWER_MANAGER_H_
#define PLATFORM_API_POWER_MANAGER_H_

#include <atomic>
#include <memory>

#include "platform/api/power_save_blocker.h"

namespace openscreen {
namespace platform {
class PowerManager {
 public:
  PowerManager() = default;
  ~PowerManager() = default;

  PowerManager(const PowerManager&) = delete;
  PowerManager(PowerManager&&) = delete;

  void RequestWakeLock();
  void ReleaseWakeLock();

 private:
  static std::atomic_int wake_lock_count_;
  static std::unique_ptr<PowerSaveBlocker> wait_lock_;
};
}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_POWER_MANAGER_H_
