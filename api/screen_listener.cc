// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/screen_listener.h"

namespace openscreen {

ScreenListenerErrorInfo::ScreenListenerErrorInfo() = default;
ScreenListenerErrorInfo::ScreenListenerErrorInfo(ScreenListenerError error,
                                                 const std::string& message)
    : error(error), message(message) {}
ScreenListenerErrorInfo::ScreenListenerErrorInfo(
    const ScreenListenerErrorInfo& other) = default;
ScreenListenerErrorInfo::ScreenListenerErrorInfo(
    ScreenListenerErrorInfo&& other) = default;
ScreenListenerErrorInfo::~ScreenListenerErrorInfo() = default;

ScreenListenerErrorInfo& ScreenListenerErrorInfo::operator=(
    const ScreenListenerErrorInfo& other) = default;
ScreenListenerErrorInfo& ScreenListenerErrorInfo::operator=(
    ScreenListenerErrorInfo&& other) = default;

ScreenListenerMetrics::ScreenListenerMetrics() = default;
ScreenListenerMetrics::~ScreenListenerMetrics() = default;

ScreenListener::ScreenListener() = default;
ScreenListener::~ScreenListener() = default;

std::ostream& operator<<(std::ostream& os, ScreenListenerState state) {
  switch (state) {
    case ScreenListenerState::kStopped:
      os << "STOPPED";
      break;
    case ScreenListenerState::kStarting:
      os << "STARTING";
      break;
    case ScreenListenerState::kRunning:
      os << "RUNNING";
      break;
    case ScreenListenerState::kStopping:
      os << "STOPPING";
      break;
    case ScreenListenerState::kSearching:
      os << "SEARCHING";
      break;
    case ScreenListenerState::kSuspended:
      os << "SUSPENDED";
      break;
  }
  return os;
}

}  // namespace openscreen
