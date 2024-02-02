// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/test/trace_logging_helpers.h"

namespace openscreen {

MockLoggingPlatform::MockLoggingPlatform() {
  StartTracing(this);

  ON_CALL(*this, IsTraceLoggingEnabled(::testing::_))
      .WillByDefault(::testing::Return(true));
}

MockLoggingPlatform::~MockLoggingPlatform() {
  StopTracing();
}

}  // namespace openscreen
