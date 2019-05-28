// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TRACE_LOGGING_TYPES_H_
#define PLATFORM_API_TRACE_LOGGING_TYPES_H_

#include "absl/types/optional.h"

namespace openscreen {

// Define TraceId type here since other TraceLogging files import it.
using TraceId = uint64_t;

// Constants used in tracing.
constexpr TraceId kEmptyTraceId = 0x0;

// A class to represent the current TraceId Hirearchy and for the user to
// pass around as needed.
struct TraceIdHierarchy {
  absl::optional<TraceId> current;
  absl::optional<TraceId> parent;
  absl::optional<TraceId> root;

  static TraceIdHierarchy Empty() {
    return {kEmptyTraceId, kEmptyTraceId, kEmptyTraceId};
  }
};

// BitFlags to represent the supported tracing categories.
struct TraceCategory {
  enum Value {
    Any = std::numeric_limits<uint64_t>::max(),
    CastPlatformLayer = 0x01,
    CastStreaming = 0x01 << 1,
    CastFlinging = 0x01 << 2
  };
};

}  // namespace openscreen

#endif
