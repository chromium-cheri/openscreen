// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_INTERNAL_TRACE_LOGGING_MACROS_INTERNAL_H_
#define PLATFORM_API_INTERNAL_TRACE_LOGGING_MACROS_INTERNAL_H_

#include "platform/api/internal/trace_logging_internal.h"
#include "platform/api/trace_logging_types.h"

namespace openscreen {

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

// Helper macros. These are used to simplify the macros below.
// NOTE: These cannot be #undef'd or they will stop working outside this file.
// NOTE: Two of these below macros are intentionally the same. This is to work
// around optimizations in the C++ Precompiler.
#define TRACE_INTERNAL_CONCAT(a, b) a##b
#define TRACE_INTERNAL_CONCAT_CONST(a, b) TRACE_INTERNAL_CONCAT(a, b)
#define TRACE_INTERNAL_UNIQUE_VAR_NAME(a) \
  TRACE_INTERNAL_CONCAT_CONST(a, __LINE__)
#define TRACE_INTERNAL_GET_TYPE(line, ...) decltype(__VA_ARGS__)

// Define a macro to check if tracing is enabled or not for testing and
// compilation reasons.
#ifndef TRACE_FORCE_ENABLE
#define TRACE_IS_ENABLED(category) \
  openscreen::platform::IsTraceLoggingEnabled(TraceCategory::Value::Any)
#ifndef ENABLE_TRACE_LOGGING
#define TRACE_FORCE_DISABLE true
#endif  // ENABLE_TRACE_LOGGING
#else   // TRACE_FORCE_ENABLE defined
#define TRACE_IS_ENABLED(category) true
#endif

// Macros and templates used to get the compile-time type information about
// calls to these trace logging methods.
template <typename... Args>
struct TraceTypeInformation {
  using Type1 = void;
  using Type2 = void;
};

template <typename TName1, typename TArg1, typename... Args>
struct TraceTypeInformation<
    TName1,
    TArg1,
    typename std::enable_if<std::is_same<TName1, const char*>::value>::type,
    Args...> {
  using Type1 = TArg1;
  using Type2 = void;
};

template <typename TName1,
          typename TArg1,
          typename TName2,
          typename TArg2,
          typename... Args>
struct TraceTypeInformation<
    TName1,
    TArg1,
    TName2,
    TArg2,
    typename std::enable_if<std::is_same<TName1, const char*>::value &&
                            std::is_same<TName2, const char*>::value>::type,
    Args...> {
  using Type1 = TArg1;
  using Type2 = TArg2;
};

#define TRACE_INTERNAL_COMMA ,
#define TRACE_INTERNAL_ARG_10(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, \
                              arg9, arg10, ...)                               \
  arg10
#define TRACE_INTERNAL_GET_TYPES_CHOOSER(...)                           \
  TRACE_INTERNAL_ARG_10(                                                \
      __VA_ARGS__, TRACE_INTERNAL_GET_TYPES_MIDDLE,                     \
      TRACE_INTERNAL_GET_TYPES_MIDDLE, TRACE_INTERNAL_GET_TYPES_MIDDLE, \
      TRACE_INTERNAL_GET_TYPES_MIDDLE, TRACE_INTERNAL_GET_TYPES_MIDDLE, \
      TRACE_INTERNAL_GET_TYPES_MIDDLE, TRACE_INTERNAL_GET_TYPES_MIDDLE, \
      TRACE_INTERNAL_GET_TYPES_MIDDLE, TRACE_INTERNAL_GET_TYPES_END,    \
      TRACE_INTERNAL_GET_TYPES_NONE)
#define TRACE_INTERNAL_GET_TYPES_(...) \
  TRACE_INTERNAL_GET_TYPES_CHOOSER(__VA_ARGS__)(__VA_ARGS__)
#define TRACE_INTERNAL_GET_TYPES_MIDDLE(arg, ...) \
  decltype(arg)##TRACE_INTERNAL_COMMA##TRACE_INTERNAL_GET_TYPES_(__VA_ARGS__)
#define TRACE_INTERNAL_GET_TYPES_END(arg) decltype(arg)
#define TRACE_INTERNAL_GET_TYPES_NONE(...)
#define TRACE_INTERNAL_TYPE_INFORMATION(...) \
  TraceTypeInformation<TRACE_INTERNAL_GET_TYPES_(__VA_ARGS__)>

// Using statements to simplify the below macros.
using openscreen::platform::internal::AsynchronousTraceLogger;
using openscreen::platform::internal::SynchronousTraceLogger;
using openscreen::platform::internal::TraceIdSetter;

// Internal logging macros.
#define TRACE_SCOPED_INTERNAL(line, category, name, ...)            \
  using TRACE_INTERNAL_CONCAT_CONST(creation_types, line) =         \
      SynchronousTraceLogger<                                       \
          TRACE_INTERNAL_TYPE_INFORMATION(__VA_ARGS__)::Type1,      \
          TRACE_INTERNAL_TYPE_INFORMATION(__VA_ARGS__)::Type2>;     \
  TRACE_INTERNAL_IGNORE_UNUSED_VAR                                  \
  const auto TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref_) =           \
      TRACE_IS_ENABLED(category)                                    \
          ? TRACE_INTERNAL_CONCAT_CONST(creation_types, line) >     \
                (category, name, __FILE__, __LINE__, ##__VA_ARGS__) \
          : TRACE_INTERNAL_CONCAT_CONST(creation_types, line) > ()

#define TRACE_ASYNC_START_INTERNAL(line, category, name, ...)       \
  using TRACE_INTERNAL_CONCAT_CONST(creation_types, line) =         \
      AsynchronousTraceLogger<                                      \
          TRACE_INTERNAL_TYPE_INFORMATION(__VA_ARGS__)::Type1,      \
          TRACE_INTERNAL_TYPE_INFORMATION(__VA_ARGS__)::Type2>;     \
  TRACE_INTERNAL_IGNORE_UNUSED_VAR                                  \
  const auto TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref_) =           \
      TRACE_IS_ENABLED(category)                                    \
          ? TRACE_INTERNAL_CONCAT_CONST(creation_types, line) >     \
                (category, name, __FILE__, __LINE__, ##__VA_ARGS__) \
          : TRACE_INTERNAL_CONCAT_CONST(creation_types, line) > ()

}  // namespace openscreen

#endif  // PLATFORM_API_INTERNAL_TRACE_LOGGING_MACROS_INTERNAL_H_
