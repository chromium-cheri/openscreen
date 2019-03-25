// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/logging.h"

#include "base/logging.h"

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
  ::logging::LogSeverity severity = ::logging::LOG_VERBOSE;
  switch (level) {
    case LogLevel::kVerbose:
      severity = ::logging::LOG_VERBOSE;
      break;
    case LogLevel::kInfo:
      severity = ::logging::LOG_INFO;
      break;
    case LogLevel::kWarning:
      severity = ::logging::LOG_WARNING;
      break;
    case LogLevel::kError:
      severity = ::logging::LOG_ERROR;
      break;
    case LogLevel::kFatal:
      severity = ::logging::LOG_FATAL;
      break;
  }
  ::logging::LogMessage(file.data(), line, severity).stream() << msg;
}

void Break() {
#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
  IMMEDIATE_CRASH();
#else
  ::base::debug::BreakDebugger();
#endif
}

}  // namespace platform
}  // namespace openscreen
