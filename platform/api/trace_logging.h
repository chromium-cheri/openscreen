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
#define TRACE_INTERNAL_CONCAT_CONST(a, b) TRACE_INTERNAL_CONCAT(a, b)
#define TRACE_INTERNAL_UNIQUE_VAR_NAME(a) \
  TRACE_INTERNAL_CONCAT_CONST(a, __LINE__)

// Because we need to suppress unused variables, and this code is used
// repeatedly in below macros, define helper macros to do this on a per-compiler
// basis until we begin using C++ 17 which supports [[maybe_unused]] officially.
#if defined(__clang__)
#define TRACE_INTERNAL_IGNORE_UNUSED_VAR [[maybe_unused]]
#elif defined(__GNUC__)
#define TRACE_INTERNAL_IGNORE_UNUSED_VAR __attribute__((unused))
#else
#define TRACE_INTERNAL_IGNORE_UNUSED_VAR [[maybe_unused]]
#endif  // defined(__clang__)

#if defined(__clang__) && defined(__clang_major__) && __clang_major__ < 4
#define TRACE_INTERNAL_IS_OUTDATED_COMPILER true
#else
#define TRACE_INTERNAL_IS_OUTDATED_COMPILER false
#endif

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
using openscreen::platform::internal::AsynchronousTraceLogger;
using openscreen::platform::internal::SynchronousTraceLogger;
using openscreen::platform::internal::TraceIdSetter;
using openscreen::platform::internal::TraceInstanceHelper;

#define TRACE_SET_RESULT(result)                                               \
  do {                                                                         \
    if (openscreen::platform::IsTraceLoggingEnabled(                           \
            TraceCategory::Value::Any)) {                                      \
      openscreen::platform::internal::ScopedTraceOperation::SetResult(result); \
    }                                                                          \
  } while (false)
#define TRACE_SET_HIERARCHY(ids) TRACE_SET_HIERARCHY_INTERNAL(__LINE__, ids)
#define TRACE_SET_HIERARCHY_INTERNAL(line, ids)                              \
  alignas(32) uint8_t TRACE_INTERNAL_CONCAT_CONST(                           \
      temp_storage,                                                          \
      line)[sizeof(openscreen::platform::internal::TraceIdSetter)];          \
  TRACE_INTERNAL_IGNORE_UNUSED_VAR                                           \
  const auto TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref_) =                    \
      openscreen::platform::IsTraceLoggingEnabled(TraceCategory::Value::Any) \
          ? TraceInstanceHelper<TraceIdSetter>::Create(                      \
                TRACE_INTERNAL_CONCAT_CONST(temp_storage, line), ids)        \
          : TraceInstanceHelper<TraceIdSetter>::Empty()
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
#define TRACE_SCOPED(category, name, ...) \
  TRACE_SCOPED_INTERNAL(__LINE__, category, name, ##__VA_ARGS__)
#define TRACE_SCOPED_INTERNAL(line, category, name, ...)                     \
  alignas(32) uint8_t TRACE_INTERNAL_CONCAT_CONST(                           \
      temp_storage,                                                          \
      line)[sizeof(openscreen::platform::internal::SynchronousTraceLogger)]; \
  TRACE_INTERNAL_IGNORE_UNUSED_VAR                                           \
  const auto TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref_) =                    \
      openscreen::platform::IsTraceLoggingEnabled(TraceCategory::Value::Any) \
          ? TraceInstanceHelper<SynchronousTraceLogger>::Create(             \
                TRACE_INTERNAL_CONCAT_CONST(temp_storage, line), category,   \
                name, __FILE__, __LINE__, ##__VA_ARGS__)                     \
          : TraceInstanceHelper<SynchronousTraceLogger>::Empty()

// Asynchronous Trace Macros.
#define TRACE_ASYNC_START(category, name, ...) \
  TRACE_ASYNC_START_INTERNAL(__LINE__, category, name, ##__VA_ARGS__)
#define TRACE_ASYNC_START_INTERNAL(line, category, name, ...)                 \
  alignas(32) uint8_t TRACE_INTERNAL_CONCAT_CONST(                            \
      temp_storage,                                                           \
      line)[sizeof(openscreen::platform::internal::AsynchronousTraceLogger)]; \
  TRACE_INTERNAL_IGNORE_UNUSED_VAR                                            \
  const auto TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref_) =                     \
      openscreen::platform::IsTraceLoggingEnabled(TraceCategory::Value::Any)  \
          ? TraceInstanceHelper<AsynchronousTraceLogger>::Create(             \
                TRACE_INTERNAL_CONCAT_CONST(temp_storage, line), category,    \
                name, __FILE__, __LINE__, ##__VA_ARGS__)                      \
          : TraceInstanceHelper<AsynchronousTraceLogger>::Empty()

#define TRACE_ASYNC_END(category, id, result)                     \
  openscreen::platform::IsTraceLoggingEnabled(category)           \
      ? openscreen::platform::internal::TraceBase::TraceAsyncEnd( \
            __LINE__, __FILE__, id, result)                       \
      : false

}  // namespace openscreen

#endif  // PLATFORM_API_TRACE_LOGGING_H_
