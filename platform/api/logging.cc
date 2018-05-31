// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {

LogMessage::LogMessage(LogLevel level, const char* file, int line)
    : level_(level), file_(file), line_(line) {}

LogMessage::~LogMessage() {
  LogWithLevel(level_, file_, line_, stream_.str().c_str());
}

LogMessageVerbose::LogMessageVerbose(int level, const char* file, int line)
    : level_(level), file_(file), line_(line) {}

LogMessageVerbose::~LogMessageVerbose() {
  LogWithLevelVerbose(level_, file_, line_, stream_.str().c_str());
}

}  // namespace platform
}  // namespace openscreen
