// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/scoped_wake_lock.h"

#if !defined(MAC_OSX)
#include "platform/api/serial_delete_ptr.h"
#include "util/osp_logging.h"
#endif  // !defined(MAC_OSX)

namespace openscreen {

class TaskRunner;

ScopedWakeLock::ScopedWakeLock() = default;
ScopedWakeLock::~ScopedWakeLock() = default;

#if !defined(MAC_OSX)
SerialDeletePtr<ScopedWakeLock> ScopedWakeLock::Create(
    TaskRunner& task_runner) {
  OSP_NOTREACHED();
}
#endif  // !defined(MAC_OSX)

}  // namespace openscreen
