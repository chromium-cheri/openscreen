// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/trace_logging.h"

#include <chrono>
#include <iostream>
#include <thread>

#include "absl/types/optional.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace platform {
namespace internal {

using ::testing::_;
using ::testing::Invoke;

// Mocks needed for testing purposes.
class MockSyncTraceLogger : public SynchronousTraceLogger {
 public:
  MockSyncTraceLogger(TraceLoggingPlatform* test_platform,
                      const char* name,
                      const char* file,
                      uint32_t line,
                      TraceCategory::Value category)
      : SynchronousTraceLogger(category, name, file, line) {
    this->platform_override_ = test_platform;
  }
  MockSyncTraceLogger(TraceLoggingPlatform* test_platform,
                      const char* name,
                      const char* file,
                      uint32_t line,
                      TraceCategory::Value category,
                      TraceId id)
      : SynchronousTraceLogger(category, name, file, line, id) {
    this->platform_override_ = test_platform;
  }
  MockSyncTraceLogger(TraceLoggingPlatform* test_platform,
                      const char* name,
                      const char* file,
                      uint32_t line,
                      TraceCategory::Value category,
                      TraceId trace_id,
                      TraceId parent_id,
                      TraceId root_id)
      : SynchronousTraceLogger(category,
                               name,
                               file,
                               line,
                               trace_id,
                               parent_id,
                               root_id) {
    this->platform_override_ = test_platform;
  }
  MockSyncTraceLogger(TraceLoggingPlatform* test_platform,
                      const char* name,
                      const char* file,
                      uint32_t line,
                      TraceCategory::Value category,
                      TraceIdHierarchy ids)
      : SynchronousTraceLogger(category, name, file, line, ids) {
    this->platform_override_ = test_platform;
  }
};

class MockAsyncTraceLogger : public AsynchronousTraceLogger {
 public:
  MockAsyncTraceLogger(TraceLoggingPlatform* test_platform,
                       const char* name,
                       const char* file,
                       uint32_t line,
                       TraceCategory::Value category)
      : AsynchronousTraceLogger(category, name, file, line) {
    this->platform_override_ = test_platform;
  }
  MockAsyncTraceLogger(TraceLoggingPlatform* test_platform,
                       const char* name,
                       const char* file,
                       uint32_t line,
                       TraceCategory::Value category,
                       TraceId id)
      : AsynchronousTraceLogger(category, name, file, line, id) {
    this->platform_override_ = test_platform;
  }
  MockAsyncTraceLogger(TraceLoggingPlatform* test_platform,
                       const char* name,
                       const char* file,
                       uint32_t line,
                       TraceCategory::Value category,
                       TraceId trace_id,
                       TraceId parent_id,
                       TraceId root_id)
      : AsynchronousTraceLogger(category,
                                name,
                                file,
                                line,
                                trace_id,
                                parent_id,
                                root_id) {
    this->platform_override_ = test_platform;
  }
  MockAsyncTraceLogger(TraceLoggingPlatform* test_platform,
                       const char* name,
                       const char* file,
                       uint32_t line,
                       TraceCategory::Value category,
                       TraceIdHierarchy ids)
      : AsynchronousTraceLogger(category, name, file, line, ids) {
    this->platform_override_ = test_platform;
  }
};

class MockLoggingPlatform : public TraceLoggingPlatform {
 public:
  MOCK_METHOD9(LogTrace,
               void(const char*,
                    const uint32_t,
                    const char* file,
                    Clock::time_point,
                    Clock::time_point,
                    TraceId,
                    TraceId,
                    TraceId,
                    Error::Code));
  MOCK_METHOD7(LogAsyncStart,
               void(const char*,
                    const uint32_t,
                    const char* file,
                    Clock::time_point,
                    TraceId,
                    TraceId,
                    TraceId));
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
                                TraceId trace_id,
                                TraceId parent_id,
                                TraceId root_id,
                                Error error) {
  uint64_t end_ms = end_time.time_since_epoch().count();
  uint64_t start_ms = start_time.time_since_epoch().count();
  EXPECT_GE(end_ms - start_ms, milliseconds);
}

template <Error::Code result>
void ValidateTraceErrorCode(const char* name,
                            const uint32_t line,
                            const char* file,
                            Clock::time_point start_time,
                            Clock::time_point end_time,
                            TraceId trace_id,
                            TraceId parent_id,
                            TraceId root_id,
                            Error error) {
  EXPECT_EQ(error.code(), result);
}

// Test methods. Because the macros always calling TraceBase::trace_platform,
// testing will skip over the macros and directly call the relevant classes.
// Since TraceBase's IsLoggingEnabled(...) and TraceAsyncEnd(...) do no work on
// their own and only directly call the platform layer, this should not leave
// a large hole in testing.
constexpr TraceCategory::Value category =
    TraceCategory::Value::CastPlatformLayer;
constexpr uint32_t line = 10;

TEST(TraceLoggingTest, TestMacroStyleInitializationTrue) {
  constexpr uint32_t delay_in_ms = 50;
  MockLoggingPlatform mock_platform;
  TraceLoggingPlatform* platform_ptr = &mock_platform;
  EXPECT_CALL(mock_platform, LogTrace(_, _, _, _, _, _, _, _, _))
      .Times(1)
      .WillOnce(DoAll(Invoke(ValidateTraceTimestampDiff<delay_in_ms>),
                      Invoke(ValidateTraceErrorCode<Error::Code::kNone>)));

  {
    uint8_t temp[sizeof(MockSyncTraceLogger)];
    TraceInstance ptr =
        true ? TraceInstance(new (temp) MockSyncTraceLogger(
                   platform_ptr, "Name", __FILE__, line, category))
             : TraceInstance();
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_in_ms));
  }
}

TEST(TraceLoggingTest, TestMacroStyleInitializationFalse) {
  MockLoggingPlatform mock_platform;
  TraceLoggingPlatform* platform_ptr = &mock_platform;
  EXPECT_CALL(mock_platform, LogTrace(_, _, _, _, _, _, _, _, _)).Times(0);

  {
    uint8_t temp[sizeof(MockSyncTraceLogger)];
    TraceInstance ptr =
        false ? TraceInstance(new (temp) MockSyncTraceLogger(
                    platform_ptr, "Name", __FILE__, line, category))
              : TraceInstance();
  }
}

TEST(TraceLoggingTest, ExpectTimestampsReflectDelay) {
  constexpr uint32_t delay_in_ms = 50;
  MockLoggingPlatform mock_platform;
  TraceLoggingPlatform* platform_ptr = &mock_platform;
  EXPECT_CALL(mock_platform, LogTrace(_, _, _, _, _, _, _, _, _))
      .WillOnce(DoAll(Invoke(ValidateTraceTimestampDiff<delay_in_ms>),
                      Invoke(ValidateTraceErrorCode<Error::Code::kNone>)));

  {
    TRACE_INTERNAL_IGNORE_UNUSED_VAR const TraceBase&
        TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) =
            MockSyncTraceLogger(platform_ptr, "Name", __FILE__, line, category);
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_in_ms));
  }
}

TEST(TraceLoggingTest, ExpectErrorsPassedToResult) {
  constexpr Error::Code result_code = Error::Code::kParseError;
  MockLoggingPlatform mock_platform;
  TraceLoggingPlatform* platform_ptr = &mock_platform;
  EXPECT_CALL(mock_platform,
              LogTrace(testing::StrEq("Name"), line, testing::StrEq(__FILE__),
                       _, _, _, _, _, _))
      .WillOnce(Invoke(ValidateTraceErrorCode<result_code>));

  {
    TRACE_INTERNAL_IGNORE_UNUSED_VAR const TraceBase&
        TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) =
            MockSyncTraceLogger{platform_ptr, "Name", __FILE__, line, category};
    ScopedTraceOperation::SetResult(result_code);
  }
}

TEST(TraceLoggingTest, ExpectUnsetTraceIdNotSet) {
  MockLoggingPlatform mock_platform;
  TraceLoggingPlatform* platform_ptr = &mock_platform;
  EXPECT_CALL(mock_platform,
              LogTrace(testing::StrEq("Name"), line, testing::StrEq(__FILE__),
                       _, _, _, _, _, _))
      .Times(1);

  TraceIdHierarchy h = {kUnsetTraceId, kUnsetTraceId, kUnsetTraceId};
  {
    TRACE_INTERNAL_IGNORE_UNUSED_VAR const TraceBase&
        TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) = MockSyncTraceLogger(
            platform_ptr, "Name", __FILE__, line, category, h);
    auto ids = ScopedTraceOperation::GetHierarchy();
    EXPECT_NE(ids.current, kUnsetTraceId);
    EXPECT_NE(ids.parent, kUnsetTraceId);
    EXPECT_NE(ids.root, kUnsetTraceId);
  }
}

TEST(TraceLoggingTest, ExpectCreationWithIdsToWork) {
  constexpr TraceId current = 0x32;
  constexpr TraceId parent = 0x47;
  constexpr TraceId root = 0x84;
  MockLoggingPlatform mock_platform;
  TraceLoggingPlatform* platform_ptr = &mock_platform;
  EXPECT_CALL(mock_platform,
              LogTrace(testing::StrEq("Name"), line, testing::StrEq(__FILE__),
                       _, _, current, parent, root, _))
      .WillOnce(Invoke(ValidateTraceErrorCode<Error::Code::kNone>));

  {
    TraceIdHierarchy h = {current, parent, root};
    TRACE_INTERNAL_IGNORE_UNUSED_VAR const TraceBase&
        TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) = MockSyncTraceLogger(
            platform_ptr, "Name", __FILE__, line, category, h);
    auto ids = ScopedTraceOperation::GetHierarchy();
    EXPECT_EQ(ids.current, current);
    EXPECT_EQ(ids.parent, parent);
    EXPECT_EQ(ids.root, root);
    EXPECT_EQ(ScopedTraceOperation::GetCurrentId(), current);
    EXPECT_EQ(ScopedTraceOperation::GetRootId(), root);
  }
}

TEST(TraceLoggingTest, ExpectHirearchyToBeApplied) {
  constexpr TraceId current = 0x32;
  constexpr TraceId parent = 0x47;
  constexpr TraceId root = 0x84;
  MockLoggingPlatform mock_platform;
  TraceLoggingPlatform* platform_ptr = &mock_platform;
  EXPECT_CALL(mock_platform,
              LogTrace(testing::StrEq("Name"), line, testing::StrEq(__FILE__),
                       _, _, _, current, root, _))
      .WillOnce(Invoke(ValidateTraceErrorCode<Error::Code::kNone>));
  EXPECT_CALL(mock_platform,
              LogTrace(testing::StrEq("Name"), line, testing::StrEq(__FILE__),
                       _, _, current, parent, root, _))
      .WillOnce(Invoke(ValidateTraceErrorCode<Error::Code::kNone>));

  {
    TRACE_INTERNAL_IGNORE_UNUSED_VAR const TraceBase&
        TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) = MockSyncTraceLogger{
            platform_ptr, "Name",   __FILE__,
            line,         category, {current, parent, root}};
    auto ids = ScopedTraceOperation::GetHierarchy();
    EXPECT_EQ(ids.current, current);
    EXPECT_EQ(ids.parent, parent);
    EXPECT_EQ(ids.root, root);

    TRACE_INTERNAL_IGNORE_UNUSED_VAR const TraceBase&
        TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) =
            MockSyncTraceLogger{platform_ptr, "Name", __FILE__, line, category};
    ids = ScopedTraceOperation::GetHierarchy();
    EXPECT_NE(ids.current, current);
    EXPECT_EQ(ids.parent, current);
    EXPECT_EQ(ids.root, root);
  }
}

TEST(TraceLoggingTest, ExpectHirearchyToEndAfterScopeWhenSetWithSetter) {
  constexpr TraceId current = 0x32;
  constexpr TraceId parent = 0x47;
  constexpr TraceId root = 0x84;
  MockLoggingPlatform mock_platform;
  TraceLoggingPlatform* platform_ptr = &mock_platform;
  EXPECT_CALL(mock_platform,
              LogTrace(testing::StrEq("Name"), line, testing::StrEq(__FILE__),
                       _, _, _, current, root, _))
      .WillOnce(Invoke(ValidateTraceErrorCode<Error::Code::kNone>));

  {
    TRACE_INTERNAL_IGNORE_UNUSED_VAR const TraceBase&
        TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) =
            TraceIdSetter{{current, parent, root}};
    {
      TRACE_INTERNAL_IGNORE_UNUSED_VAR const TraceBase&
          TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) = MockSyncTraceLogger{
              platform_ptr, "Name", __FILE__, line, category};
      auto ids = ScopedTraceOperation::GetHierarchy();
      EXPECT_NE(ids.current, current);
      EXPECT_EQ(ids.parent, current);
      EXPECT_EQ(ids.root, root);
    }

    auto ids = ScopedTraceOperation::GetHierarchy();
    EXPECT_EQ(ids.current, current);
    EXPECT_EQ(ids.parent, parent);
    EXPECT_EQ(ids.root, root);
  }
}

TEST(TraceLoggingTest, ExpectHirearchyToEndAfterScope) {
  constexpr TraceId current = 0x32;
  constexpr TraceId parent = 0x47;
  constexpr TraceId root = 0x84;
  MockLoggingPlatform mock_platform;
  TraceLoggingPlatform* platform_ptr = &mock_platform;
  EXPECT_CALL(mock_platform,
              LogTrace(testing::StrEq("Name"), line, testing::StrEq(__FILE__),
                       _, _, _, current, root, _))
      .WillOnce(Invoke(ValidateTraceErrorCode<Error::Code::kNone>));
  EXPECT_CALL(mock_platform,
              LogTrace(testing::StrEq("Name"), line, testing::StrEq(__FILE__),
                       _, _, current, parent, root, _))
      .WillOnce(Invoke(ValidateTraceErrorCode<Error::Code::kNone>));

  {
    TRACE_INTERNAL_IGNORE_UNUSED_VAR const TraceBase&
        TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) = MockSyncTraceLogger{
            platform_ptr, "Name",   __FILE__,
            line,         category, {current, parent, root}};
    {
      TRACE_INTERNAL_IGNORE_UNUSED_VAR const TraceBase&
          TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) = MockSyncTraceLogger{
              platform_ptr, "Name", __FILE__, line, category};
      auto ids = ScopedTraceOperation::GetHierarchy();
      EXPECT_NE(ids.current, current);
      EXPECT_EQ(ids.parent, current);
      EXPECT_EQ(ids.root, root);
    }

    auto ids = ScopedTraceOperation::GetHierarchy();
    EXPECT_EQ(ids.current, current);
    EXPECT_EQ(ids.parent, parent);
    EXPECT_EQ(ids.root, root);
  }
}

TEST(TraceLoggingTest, ExpectSetHierarchyToApply) {
  constexpr TraceId current = 0x32;
  constexpr TraceId parent = 0x47;
  constexpr TraceId root = 0x84;
  MockLoggingPlatform mock_platform;
  TraceLoggingPlatform* platform_ptr = &mock_platform;
  EXPECT_CALL(mock_platform,
              LogTrace(testing::StrEq("Name"), line, testing::StrEq(__FILE__),
                       _, _, _, current, root, _))
      .WillOnce(Invoke(ValidateTraceErrorCode<Error::Code::kNone>));

  {
    TRACE_INTERNAL_IGNORE_UNUSED_VAR const TraceBase&
        TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) =
            TraceIdSetter{{current, parent, root}};
    auto ids = ScopedTraceOperation::GetHierarchy();
    EXPECT_EQ(ids.current, current);
    EXPECT_EQ(ids.parent, parent);
    EXPECT_EQ(ids.root, root);

    TRACE_INTERNAL_IGNORE_UNUSED_VAR const TraceBase&
        TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) =
            MockSyncTraceLogger{platform_ptr, "Name", __FILE__, line, category};
    ids = ScopedTraceOperation::GetHierarchy();
    EXPECT_NE(ids.current, current);
    EXPECT_EQ(ids.parent, current);
    EXPECT_EQ(ids.root, root);
  }
}

TEST(TraceLoggingTest, CheckTraceAsyncStartLogsCorrectly) {
  MockLoggingPlatform mock_platform;
  TraceLoggingPlatform* platform_ptr = &mock_platform;
  EXPECT_CALL(mock_platform,
              LogAsyncStart(testing::StrEq("Name"), line,
                            testing::StrEq(__FILE__), _, _, _, _))
      .Times(1);

  { MockAsyncTraceLogger{platform_ptr, "Name", __FILE__, line, category}; }
}

TEST(TraceLoggingTest, CheckTraceAsyncStartSetsHierarchy) {
  constexpr TraceId current = 32;
  constexpr TraceId parent = 47;
  constexpr TraceId root = 84;
  MockLoggingPlatform mock_platform;
  TraceLoggingPlatform* platform_ptr = &mock_platform;
  EXPECT_CALL(mock_platform,
              LogAsyncStart(testing::StrEq("Name"), line,
                            testing::StrEq(__FILE__), _, _, current, root))
      .Times(1);

  {
    TRACE_INTERNAL_IGNORE_UNUSED_VAR const TraceBase&
        TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) =
            TraceIdSetter{{current, parent, root}};
    {
      TRACE_INTERNAL_IGNORE_UNUSED_VAR const TraceBase&
          TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) = MockAsyncTraceLogger{
              platform_ptr, "Name", __FILE__, line, category};
      auto ids = ScopedTraceOperation::GetHierarchy();
      EXPECT_NE(ids.current, current);
      EXPECT_EQ(ids.parent, current);
      EXPECT_EQ(ids.root, root);
    }

    auto ids = ScopedTraceOperation::GetHierarchy();
    EXPECT_EQ(ids.current, current);
    EXPECT_EQ(ids.parent, parent);
    EXPECT_EQ(ids.root, root);
  }
}

}  // namespace internal
}  // namespace platform
}  // namespace openscreen
