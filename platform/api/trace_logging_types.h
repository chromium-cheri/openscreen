// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TRACE_LOGGING_TYPES_H_
#define PLATFORM_API_TRACE_LOGGING_TYPES_H_

#include "absl/types/optional.h"
#include "platform/base/macros.h"

namespace openscreen {

// Define TraceId type here since other TraceLogging files import it.
using TraceId = uint64_t;

// kEmptyTraceId is the Trace ID when tracing at a global level, not inside any
// tracing block - ie this will be the parent ID for a top level tracing block.
constexpr TraceId kEmptyTraceId = 0x0;

// kUnsetTraceId is the Trace ID passed in to the tracing library when no user-
// specified value is desired.
constexpr TraceId kUnsetTraceId = std::numeric_limits<TraceId>::max();

// A class to represent the current TraceId Hirearchy and for the user to
// pass around as needed.
struct TraceIdHierarchy {
  TraceId current;
  TraceId parent;
  TraceId root;

  static constexpr TraceIdHierarchy Empty() {
    return {kEmptyTraceId, kEmptyTraceId, kEmptyTraceId};
  }

  bool HasCurrent() { return current != kUnsetTraceId; }
  bool HasParent() { return parent != kUnsetTraceId; }
  bool HasRoot() { return root != kUnsetTraceId; }
};
inline bool operator==(const TraceIdHierarchy& lhs,
                       const TraceIdHierarchy& rhs) {
  return lhs.current == rhs.current && lhs.parent == rhs.parent &&
         lhs.root == rhs.root;
}
inline bool operator!=(const TraceIdHierarchy& lhs,
                       const TraceIdHierarchy& rhs) {
  return !(lhs == rhs);
}

// BitFlags to represent the supported tracing categories.
// NOTE: These are currently placeholder values and later changes should feel
// free to edit them.
struct TraceCategory {
  enum Value : uint64_t {
    Any = std::numeric_limits<uint64_t>::max(),
    mDNS = 0x01 << 0,
    Quic = 0x01 << 1,
    Presentation = 0x01 << 2,
  };
};

// Type used for user provided arguments.
class UserArgumentValue {
 public:
  union Data {
    const char* string;
    double floating_point;
    int64_t integer;
  };

  enum class DataType { kString, kFloatingPoint, kInteger };

  explicit UserArgumentValue(const char* string)
      : data_({.string = string}), data_type_(DataType::kString) {}

  explicit UserArgumentValue(double floating_point)
      : data_({.floating_point = floating_point}),
        data_type_(DataType::kFloatingPoint) {}

  explicit UserArgumentValue(int64_t integer)
      : data_({.integer = integer}), data_type_(DataType::kInteger) {}

  UserArgumentValue(const UserArgumentValue& other) = default;
  UserArgumentValue(UserArgumentValue&& other) = default;
  UserArgumentValue& operator=(UserArgumentValue&& other) = default;
  UserArgumentValue& operator=(const UserArgumentValue& other) = default;

  Data data() const { return data_; }

  DataType type() const { return data_type_; }

 private:
  Data data_;
  DataType data_type_;
};

}  // namespace openscreen

#endif  // PLATFORM_API_TRACE_LOGGING_TYPES_H_
