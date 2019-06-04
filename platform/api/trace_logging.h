// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TRACE_LOGGING_H_
#define PLATFORM_API_TRACE_LOGGING_H_

#include "platform/api/internal/trace_logging_internal.h"
#include "platform/api/trace_logging_types.h"

namespace openscreen {

// Helper macros. These are used to simplify the macros below.
// NOTE: These cannot be #undef'd or they will stop working outside this file.
// NOTE: Two of these below macros are intentionally the same. This is to work
// around optimizations in the C++ Precompiler.
#define TRACE_INTERNAL_CONCAT(a, b) a##b
#define TRACE_INTERNAL_CONCAT_DUP(a, b) TRACE_INTERNAL_CONCAT(a, b)
#define TRACE_INTERNAL_UNIQUE_VAR_NAME(a) TRACE_INTERNAL_CONCAT_DUP(a, __LINE__)

// All compile-time macros for tracing.
// NOTE: the ternary operator is used here to ensure that the TraceLogger object
// is only constructed if tracing is enabled, but at the same time is created in
// the caller's scope. The C++ standards guide guarantees that the constructor
// should only be called when IsLoggingEnabled(...) evaluates to true.
//
// Further details about how these macros are used can be found in
// docs/trace_logging.md.
#define TRACE_SET_RESULT(result)                                         \
  openscreen::platform::IsLoggingEnabled(TraceCategory::Value::Any)      \
      ? openscreen::platform::internal::ScopedTraceOperation::SetResult( \
            result)                                                      \
      : false
#define TRACE_SET_HIERARCHY(ids)                                            \
  [[maybe_unused]] const openscreen::platform::internal::TraceBase&         \
      TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref_) =                          \
          openscreen::platform::IsLoggingEnabled(TraceCategory::Value::Any) \
              ? openscreen::platform::internal::TraceIdSetter(ids)          \
              : openscreen::platform::internal::TraceBase()
#define TRACE_HIERARCHY                                                       \
  (openscreen::platform::IsLoggingEnabled(TraceCategory::Value::Any)          \
       ? openscreen::platform::internal::ScopedTraceOperation::GetHierarchy() \
       : TraceIdHierarchy::Empty())
#define TRACE_CURRENT_ID                                                      \
  (openscreen::platform::IsLoggingEnabled(TraceCategory::Value::Any)          \
       ? openscreen::platform::internal::ScopedTraceOperation::GetCurrentId() \
       : kEmptyTraceId)
#define TRACE_ROOT_ID                                                      \
  (openscreen::platform::IsLoggingEnabled(TraceCategory::Value::Any)       \
       ? openscreen::platform::internal::ScopedTraceOperation::GetRootId() \
       : kEmptyTraceId)

// Synchronous Trace Macro.
#define TRACE_SCOPED(category, name, ...)                                \
  [[maybe_unused]] const openscreen::platform::internal::TraceBase&      \
      TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref_) =                       \
          openscreen::platform::IsLoggingEnabled(category)               \
              ? openscreen::platform::internal::SynchronousTraceLogger<  \
                    category, __LINE__, sizeof(__FILE__) / sizeof(char), \
                    sizeof(name) / sizeof(char)>{name, __FILE__,         \
                                                 ##__VA_ARGS__}          \
              : openscreen::platform::internal::TraceBase()

// Asynchronous Trace Macros.
#define TRACE_ASYNC_START(category, name, ...)                           \
  [[maybe_unused]] const openscreen::platform::internal::TraceBase&      \
      TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref_) =                       \
          openscreen::platform::IsLoggingEnabled(category)               \
              ? openscreen::platform::internal::AsynchronousTraceLogger< \
                    category, __LINE__, sizeof(__FILE__) / sizeof(char), \
                    sizeof(name) / sizeof(char)>{name, __FILE__,         \
                                                 ##__VA_ARGS__}          \
              : openscreen::platform::internal::TraceBase()
#define TRACE_ASYNC_END(category, id, result)                     \
  openscreen::platform::IsLoggingEnabled(category)                \
      ? openscreen::platform::internal::TraceBase::TraceAsyncEnd( \
            __LINE__, __FILE__, id, result)                       \
      : false

}  // namespace openscreen

#endif
