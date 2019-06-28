// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/logging.h"
#include "platform/api/trace_logging.h"
#include "platform/api/trace_logging_types.h"

namespace openscreen {
namespace platform {

// clang-format off
bool IsLoggingEnabled(TraceCategory::Value category) {
  #ifndef NDEBUG
      constexpr uint64_t kAllLogCategoriesMask =
          std::numeric_limits<uint64_t>::max();
      return (kAllLogCategoriesMask & category) != 0;
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
    auto total_runtime = std::chrono::duration_cast<std::chrono::microseconds>(
                             end_time - start_time)
                             .count();
    constexpr auto microseconds_symbol = "\u03BCs";  // Greek Mu + 's'
    OSP_LOG << "TRACE [" << std::hex << root_id << ":" << parent_id << ":"
            << trace_id << "] (" << std::dec << total_runtime
            << microseconds_symbol << ") " << name << "<" << file << ":" << line
            << "> " << error.code();
  }

  void LogAsyncStart(const char* name,
                     const uint32_t line,
                     const char* file,
                     Clock::time_point timestamp,
                     TraceId trace_id,
                     TraceId parent_id,
                     TraceId root_id) {
    OSP_LOG << "ASYNC TRACE START [" << std::hex << root_id << ":" << parent_id
            << ":" << trace_id << std::dec << "] (" << timestamp << ") " << name
            << "<" << file << ":" << line << ">";
  }

  void LogAsyncEnd(const uint32_t line,
                   const char* file,
                   Clock::time_point timestamp,
                   TraceId trace_id,
                   Error error) {
    OSP_LOG << "ASYNC TRACE END [" << std::hex << trace_id << std::dec << "] ("
            << timestamp << ") " << error.code();
  }
};

TRACE_SET_DEFAULT_PLATFORM(new TextTraceLoggingPlatform);

}  // namespace platform
}  // namespace openscreen
