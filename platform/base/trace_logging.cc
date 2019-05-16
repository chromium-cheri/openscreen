// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/trace_logging.h"

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {

// clang-format off
bool IsLoggingEnabled(TraceCategory::Value category) {
  #if defined(_DEBUG)
      constexpr uint64_t kAllLogCategoriesMask =
          std::numeric_limits<uint64_t>::max();
      return kAllLogCategoriesMask & category != 0;
  #else
      return false;
  #endif
}
// clang-format on

class TextTraceLoggingPlatform : public TraceLoggingPlatform {
 public:
  TextTraceLoggingPlatform() = default;
  ~TextTraceLoggingPlatform() = default;

  void LogTrace(const char* name,
                const uint32_t line,
                const char* file,
                Clock::time_point start_time,
                Clock::time_point end_time,
                TraceId trace_id,
                TraceId parent_id,
                TraceId root_id,
                Error error) {
    OSP_VLOG << "TRACE [" << root_id << ":" << parent_id << ":" << trace_id
             << "] (" << start_time << ", " << end_time << ") " << name << "<"
             << file << ":" << line << "> " << error.code();
  }

  void LogAsyncStart(const char* name,
                     const uint32_t line,
                     const char* file,
                     Clock::time_point timestamp,
                     TraceId trace_id,
                     TraceId parent_id,
                     TraceId root_id) {
    OSP_VLOG << "ASYNC TRACE START [" << root_id << ":" << parent_id << ":"
             << trace_id << "] (" << timestamp << ") " << name << "<" << file
             << ":" << line << ">";
  }

  void LogAsyncEnd(const uint32_t line,
                   const char* file,
                   Clock::time_point timestamp,
                   TraceId trace_id,
                   Error error) {
    OSP_VLOG << "ASYNC TRACE END [" << trace_id << "] (" << timestamp << ") "
             << error.code();
  }
};

TraceLoggingPlatform* TraceBase::trace_platform =
    new TextTraceLoggingPlatform();

}  // namespace platform
}  // namespace openscreen
