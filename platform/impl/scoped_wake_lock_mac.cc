// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/scoped_wake_lock_mac.h"

#include <CoreFoundation/CoreFoundation.h>

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {

ScopedWakeLockMac::LockState ScopedWakeLockMac::lock_state_{};

std::unique_ptr<ScopedWakeLock> ScopedWakeLock::Create() {
  return std::unique_ptr<ScopedWakeLock>(new ScopedWakeLockMac());
}

ScopedWakeLockMac::ScopedWakeLockMac() {
  if (lock_state_.reference_count++ == 0) {
    CFMutableDictionaryRef assertion_properties = CFDictionaryCreateMutable(
        0, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFDictionarySetValue(assertion_properties, kIOPMAssertionTypeKey,
                         kIOPMAssertionTypeNoDisplaySleep);

    CFDictionarySetValue(assertion_properties, kIOPMAssertionNameKey,
                         CFSTR("Open Screen ScopedWakeLock"));
    const IOReturn result = IOPMAssertionCreateWithProperties(
        assertion_properties, &lock_state_.assertion_id);

    OSP_DCHECK_EQ(result, kIOReturnSuccess);
  }
}

ScopedWakeLockMac::~ScopedWakeLockMac() {
  if (--lock_state_.reference_count == 0) {
    const IOReturn result = IOPMAssertionRelease(lock_state_.assertion_id);
    OSP_DCHECK_EQ(result, kIOReturnSuccess);
  }
}

}  // namespace platform
}  // namespace openscreen
