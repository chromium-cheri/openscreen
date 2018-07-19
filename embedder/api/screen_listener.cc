// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "embedder/api/screen_listener.h"

namespace openscreen {

std::ostream& operator<<(std::ostream& os, ScreenListenerState state) {
  switch (state) {
    case ScreenListenerState::STOPPED:
      os << "STOPPED";
      break;
    case ScreenListenerState::STARTING:
      os << "STARTING";
      break;
    case ScreenListenerState::RUNNING:
      os << "RUNNING";
      break;
    case ScreenListenerState::STOPPING:
      os << "STOPPING";
      break;
    case ScreenListenerState::SEARCHING:
      os << "SEARCHING";
      break;
    case ScreenListenerState::SUSPENDED:
      os << "SUSPENDED";
      break;
  }
  return os;
}

}  // namespace openscreen
