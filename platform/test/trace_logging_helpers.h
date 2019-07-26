// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/trace_logging.h"

#ifndef PLATFORM_TEST_TRACE_LOGGING_HELPERS_H_
#define PLATFORM_TEST_TRACE_LOGGING_HELPERS_H_

namespace openscreen {
namespace platform {

enum TraceHierarchyParts { kRoot = 0x1, kParent = 0x2, kCurrent = 0x4 };

enum ArgumentId { kFirst, kSecond };

class MockLoggingPlatform : public TraceLoggingPlatform {
 public:
  MOCK_METHOD9(LogTrace,
               void(const char*,
                    const uint32_t,
                    const char* file,
                    Clock::time_point,
                    Clock::time_point,
                    TraceIdHierarchy ids,
                    Error::Code,
                    absl::optional<UserDefinedArgument>,
                    absl::optional<UserDefinedArgument>));
  MOCK_METHOD7(LogAsyncStart,
               void(const char*,
                    const uint32_t,
                    const char* file,
                    Clock::time_point,
                    TraceIdHierarchy,
                    absl::optional<UserDefinedArgument>,
                    absl::optional<UserDefinedArgument>));
  MOCK_METHOD5(LogAsyncEnd,
               void(const uint32_t,
                    const char* file,
                    Clock::time_point,
                    TraceId,
                    Error::Code));
};

// Methods to validate the results of platform-layer calls.
template <uint64_t milliseconds>
void ValidateTraceTimestampDiff(const char* name,
                                const uint32_t line,
                                const char* file,
                                Clock::time_point start_time,
                                Clock::time_point end_time,
                                TraceIdHierarchy ids,
                                Error error,
                                absl::optional<UserDefinedArgument> arg1,
                                absl::optional<UserDefinedArgument> arg2) {
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);
  ASSERT_GE(elapsed.count(), milliseconds);
}

template <Error::Code result>
void ValidateTraceErrorCode(const char* name,
                            const uint32_t line,
                            const char* file,
                            Clock::time_point start_time,
                            Clock::time_point end_time,
                            TraceIdHierarchy ids,
                            Error error,
                            absl::optional<UserDefinedArgument> arg1,
                            absl::optional<UserDefinedArgument> arg2) {
  ASSERT_EQ(error.code(), result);
}

template <TraceId Current,
          TraceId Parent,
          TraceId Root,
          TraceHierarchyParts parts>
void ValidateTraceIdHierarchyOnSyncTrace(
    const char* name,
    const uint32_t line,
    const char* file,
    Clock::time_point start_time,
    Clock::time_point end_time,
    TraceIdHierarchy ids,
    Error error,
    absl::optional<UserDefinedArgument> arg1,
    absl::optional<UserDefinedArgument> arg2) {
  if (parts & TraceHierarchyParts::kCurrent) {
    ASSERT_EQ(ids.current, Current);
  }
  if (parts & TraceHierarchyParts::kParent) {
    ASSERT_EQ(ids.parent, Parent);
  }
  if (parts & TraceHierarchyParts::kRoot) {
    ASSERT_EQ(ids.root, Root);
  }
}

template <TraceId Current,
          TraceId Parent,
          TraceId Root,
          TraceHierarchyParts parts>
void ValidateTraceIdHierarchyOnAsyncTrace(
    const char* name,
    const uint32_t line,
    const char* file,
    Clock::time_point timestamp,
    TraceIdHierarchy ids,
    absl::optional<UserDefinedArgument> arg1,
    absl::optional<UserDefinedArgument> arg2) {
  if (parts & TraceHierarchyParts::kCurrent) {
    EXPECT_EQ(ids.current, Current);
  }
  if (parts & TraceHierarchyParts::kParent) {
    EXPECT_EQ(ids.parent, Parent);
  }
  if (parts & TraceHierarchyParts::kRoot) {
    EXPECT_EQ(ids.root, Root);
  }
}

template <ArgumentId Arg>
void ValidateUserArgumentEmpty(const char* name,
                               const uint32_t line,
                               const char* file,
                               Clock::time_point start_time,
                               Clock::time_point end_time,
                               TraceIdHierarchy ids,
                               Error error,
                               absl::optional<UserDefinedArgument> arg1,
                               absl::optional<UserDefinedArgument> arg2) {
  absl::optional<UserDefinedArgument>* arg =
      Arg == ArgumentId::kFirst ? &arg1 : &arg2;

  EXPECT_FALSE(arg->has_value());
}

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_TEST_TRACE_LOGGING_HELPERS_H_
