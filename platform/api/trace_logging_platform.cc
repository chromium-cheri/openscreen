// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/trace_logging_platform.h"

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {

TraceLoggingPlatform::~TraceLoggingPlatform() = default;

// static
TraceLoggingPlatform* TraceLoggingPlatform::GetDefaultTracingPlatform() {
  return default_platform;
}

// static
void TraceLoggingPlatform::SetDefaultTraceLoggingPlatform(
    TraceLoggingPlatform* platform) {
  OSP_DCHECK(default_platform == nullptr)
      << "Default tracing platform already assigned";
  default_platform = platform;
}

// static
TraceLoggingPlatform* TraceLoggingPlatform::default_platform = nullptr;

}  // namespace platform
}  // namespace openscreen
