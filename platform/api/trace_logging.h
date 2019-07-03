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

// Because we need to suppress unused variables, and this code is used
// repeatedly in below macros, define helper macros to do this for us.
#if defined(__clang__)
#define TRACE_INTERNAL_IGNORE_UNUSED_VARS_START \
  _Pragma("clang diagnostic push");             \
  _Pragma("clang diagnostic ignored \"-Wunused-variable\"")
#define TRACE_INTERNAL_IGNORE_UNUSED_VARS_END _Pragma("clang diagnostic pop")

#elif defined(__GNUC__)
#define TRACE_INTERNAL_IGNORE_UNUSED_VARS_START \
  _Pragma("GCC diagnostic push");               \
  _Pragma("GCC diagnostic ignored \"-Wunused-variable\"")
#define TRACE_INTERNAL_IGNORE_UNUSED_VARS_END _Pragma("GCC diagnostic pop")

#else
#define TRACE_INTERNAL_IGNORE_UNUSED_VARS_START
#define TRACE_INTERNAL_IGNORE_UNUSED_VARS_END
#endif  // defined(__clang__)

// All compile-time macros for tracing.
// NOTE: The ternary operator is used here to ensure that the TraceLogger object
// is only constructed if tracing is enabled, but at the same time is created in
// the caller's scope. The C++ standards guide guarantees that the constructor
// should only be called when IsTraceLoggingEnabled(...) evaluates to true.
// static_cast calls are used because if the type of the result of the ternary
// operator does not match the expected type, temporary storage is used for the
// created object, which results in an extra call to the constructor and
// destructor of the tracing objects.
//
// Further details about how these macros are used can be found in
// docs/trace_logging.md.
// TODO(rwkeane): Add option to compile these macros out entirely.
// TODO(rwkeane): Add support for user-provided properties.
#define TRACE_SET_RESULT(result)                                               \
  do {                                                                         \
    if (openscreen::platform::IsTraceLoggingEnabled(                           \
            TraceCategory::Value::Any)) {                                      \
      openscreen::platform::internal::ScopedTraceOperation::SetResult(result); \
    }                                                                          \
  } while (false)
#define TRACE_SET_HIERARCHY(ids)                                               \
  TRACE_INTERNAL_IGNORE_UNUSED_VARS_START;                                     \
  const openscreen::platform::internal::TraceBase&                             \
      TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref_) =                             \
          openscreen::platform::IsTraceLoggingEnabled(                         \
              TraceCategory::Value::Any)                                       \
              ? static_cast<const openscreen::platform::internal::TraceBase&>( \
                    openscreen::platform::internal::TraceIdSetter(ids))        \
              : static_cast<const openscreen::platform::internal::TraceBase&>( \
                    openscreen::platform::internal::TraceBase());              \
  TRACE_INTERNAL_IGNORE_UNUSED_VARS_END
#define TRACE_HIERARCHY                                                       \
  (openscreen::platform::IsTraceLoggingEnabled(TraceCategory::Value::Any)     \
       ? openscreen::platform::internal::ScopedTraceOperation::GetHierarchy() \
       : TraceIdHierarchy::Empty())
#define TRACE_CURRENT_ID                                                      \
  (openscreen::platform::IsTraceLoggingEnabled(TraceCategory::Value::Any)     \
       ? openscreen::platform::internal::ScopedTraceOperation::GetCurrentId() \
       : kEmptyTraceId)
#define TRACE_ROOT_ID                                                      \
  (openscreen::platform::IsTraceLoggingEnabled(TraceCategory::Value::Any)  \
       ? openscreen::platform::internal::ScopedTraceOperation::GetRootId() \
       : kEmptyTraceId)

// Synchronous Trace Macro.
#define TRACE_SCOPED(category, name, ...)                                      \
  TRACE_INTERNAL_IGNORE_UNUSED_VARS_START;                                     \
  const openscreen::platform::internal::TraceBase&                             \
      TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref_) =                             \
          openscreen::platform::IsTraceLoggingEnabled(category)                \
              ? static_cast<const openscreen::platform::internal::TraceBase&>( \
                    openscreen::platform::internal::SynchronousTraceLogger{    \
                        category, name, __FILE__, __LINE__, ##__VA_ARGS__})    \
              : static_cast<const openscreen::platform::internal::TraceBase&>( \
                    openscreen::platform::internal::TraceBase());              \
  TRACE_INTERNAL_IGNORE_UNUSED_VARS_END

// Asynchronous Trace Macros.
#define TRACE_ASYNC_START(category, name, ...)                                 \
  TRACE_INTERNAL_IGNORE_UNUSED_VARS_START;                                     \
  const openscreen::platform::internal::TraceBase&                             \
      TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref_) =                             \
          openscreen::platform::IsTraceLoggingEnabled(category)                \
              ? static_cast<const openscreen::platform::internal::TraceBase&>( \
                    openscreen::platform::internal::AsynchronousTraceLogger{   \
                        category, name, __FILE__, __LINE__, ##__VA_ARGS__})    \
              : static_cast<const openscreen::platform::internal::TraceBase&>( \
                    openscreen::platform::internal::TraceBase());              \
  TRACE_INTERNAL_IGNORE_UNUSED_VARS_END
#define TRACE_ASYNC_END(category, id, result)                     \
  openscreen::platform::IsTraceLoggingEnabled(category)           \
      ? openscreen::platform::internal::TraceBase::TraceAsyncEnd( \
            __LINE__, __FILE__, id, result)                       \
      : false

}  // namespace openscreen

#endif  // PLATFORM_API_TRACE_LOGGING_H_
