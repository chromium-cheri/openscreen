// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {
namespace {

std::string LogLevelToString(LogLevel level) {
  switch (level) {
    case LogLevel::kDebug:
      return "DEBUG";
    case LogLevel::kInfo:
      return "INFO";
    case LogLevel::kWarning:
      return "WARNING";
    case LogLevel::kError:
      return "ERROR";
    default:
      return "";
  }
}

}  // namespace

void LogWithLevel(LogLevel level, const char* file, int line, const char* msg) {
  std::cout << "[" << LogLevelToString(level) << ":" << file << ":" << line
            << "] " << msg << std::endl;
}

void LogWithLevelVerbose(int level,
                         const char* file,
                         int line,
                         const char* msg) {
  std::cout << "[VERBOSE(" << level << "):" << file << ":" << line << "] "
            << msg << std::endl;
}

}  // namespace platform
}  // namespace openscreen
