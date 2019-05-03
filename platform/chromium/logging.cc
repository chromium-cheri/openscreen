// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {

// We don't initialize Chromium's logging.
void LogInit(const char* filename) {}

// We don't set Chromium's logging level.
void SetLogLevel(LogLevel level) {}

void LogWithLevel(LogLevel level,
                  absl::string_view file,
                  int line,
                  absl::string_view msg) {
  // TODO(jophba): fix logging in Chromium, currently we cannot interact
  // with the Chromium logger from third_party.
}

void Break() {
#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
  IMMEDIATE_CRASH();
#else
  // TODO(jophba): add support for breaking debugger in Chromium.
#endif
}

}  // namespace platform
}  // namespace openscreen
