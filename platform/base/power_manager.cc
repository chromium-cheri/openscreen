// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/power_manager.h"

#include "platform/api/logging.h"
#include "platform/api/power_save_blocker.h"

namespace openscreen {
namespace platform {

static std::atomic_int wake_lock_count_{};
static std::unique_ptr<PowerSaveBlocker>& wake_lock_ =
    *new std::unique_ptr<PowerSaveBlocker>();

void PowerManager::RequestWakeLock() {
  if (!wake_lock_count_) {
    wake_lock_ = std::unique_ptr<PowerSaveBlocker>(new PowerSaveBlocker());
  }
  wake_lock_count_++;
}

void PowerManager::ReleaseWakeLock() {
  const auto new_count = --wake_lock_count_;
  OSP_DCHECK_GE(new_count, 0);

  if (!new_count) {
    wake_lock_ = nullptr;
  }
}
}  // namespace platform
}  // namespace openscreen
