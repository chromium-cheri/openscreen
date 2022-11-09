// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_TRACE_LOGGING_TYPES_H_
#define PLATFORM_BASE_TRACE_LOGGING_TYPES_H_

#include <stdint.h>

#include <limits>
#include <sstream>
#include <string>

namespace openscreen {

// Define TraceId type here since other TraceLogging files import it.
using TraceId = uint64_t;

// kEmptyTraceId is the Trace ID when tracing at a global level, not inside any
// tracing block - ie this will be the parent ID for a top level tracing block.
constexpr TraceId kEmptyTraceId = 0x0;

// kUnsetTraceId is the Trace ID passed in to the tracing library when no user-
// specified value is desired.
constexpr TraceId kUnsetTraceId = std::numeric_limits<TraceId>::max();

// A class to represent the current TraceId Hierarchy and for the user to
// pass around as needed.
struct TraceIdHierarchy {
  TraceId current;
  TraceId parent;
  TraceId root;

  static constexpr TraceIdHierarchy Empty() {
    return {kEmptyTraceId, kEmptyTraceId, kEmptyTraceId};
  }

  bool HasCurrent() const;
  bool HasParent() const;
  bool HasRoot() const;

  std::string ToString() const;
};

std::ostream& operator<<(std::ostream& out, const TraceIdHierarchy& ids);

bool operator==(const TraceIdHierarchy& lhs, const TraceIdHierarchy& rhs);

bool operator!=(const TraceIdHierarchy& lhs, const TraceIdHierarchy& rhs);

// BitFlags to represent the supported tracing categories.
struct TraceCategory {
  enum Value : uint64_t {
    kAny = std::numeric_limits<uint64_t>::max(),
    kMdns = 0x01 << 0,
    kQuic = 0x01 << 1,
    kSsl = 0x01 << 2,
    kPresentation = 0x01 << 3,
    kStandaloneReceiver = 0x01 << 4,
    kDiscovery = 0x01 << 5,
    kStandaloneSender = 0x01 << 6,
    kReceiver = 0x01 << 7,
    kSender = 0x01 << 8
  };
};

const char* ToString(TraceCategory::Value category);

}  // namespace openscreen

#endif  // PLATFORM_BASE_TRACE_LOGGING_TYPES_H_
