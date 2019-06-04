// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/trace_logging.h"

#include <chrono>
#include <thread>

#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {
namespace platform {
namespace internal {

using ::testing::_;
using ::testing::Invoke;

// Mocks needed for testing purposes.
template <TraceCategory::Value TCategory,
          uint32_t TLine,
          size_t TFileSize,
          size_t TNameSize>
class MockSyncTraceLogger
    : public SynchronousTraceLogger<TCategory, TLine, TFileSize, TNameSize> {
 public:
  MockSyncTraceLogger(TraceLoggingPlatform* test_platform,
                      const char name[TNameSize],
                      const char file[TFileSize])
      : SynchronousTraceLogger<TCategory, TLine, TFileSize, TNameSize>(name,
                                                                       file) {
    this->platform = test_platform;
  }
  MockSyncTraceLogger(TraceLoggingPlatform* test_platform,
                      const char name[TNameSize],
                      const char file[TFileSize],
                      TraceId id)
      : SynchronousTraceLogger<TCategory, TLine, TFileSize, TNameSize>(name,
                                                                       id,
                                                                       file) {
    this->platform = test_platform;
  }
  MockSyncTraceLogger(TraceLoggingPlatform* test_platform,
                      const char name[TNameSize],
                      const char file[TFileSize],
                      TraceId trace_id,
                      TraceId parent_id,
                      TraceId root_id)
      : SynchronousTraceLogger<TCategory, TLine, TFileSize, TNameSize>(
            name,
            file,
            trace_id,
            parent_id,
            root_id) {
    this->platform = test_platform;
  }
  MockSyncTraceLogger(TraceLoggingPlatform* test_platform,
                      const char name[TNameSize],
                      const char file[TFileSize],
                      TraceIdHierarchy ids)
      : SynchronousTraceLogger<TCategory, TLine, TFileSize, TNameSize>(name,
                                                                       file,
                                                                       ids) {
    this->platform = test_platform;
  }
};

template <TraceCategory::Value TCategory,
          uint32_t TLine,
          size_t TFileSize,
          size_t TNameSize>
class MockAsyncTraceLogger
    : public AsynchronousTraceLogger<TCategory, TLine, TFileSize, TNameSize> {
 public:
  MockAsyncTraceLogger(TraceLoggingPlatform* test_platform,
                       const char name[TNameSize],
                       const char file[TFileSize])
      : AsynchronousTraceLogger<TCategory, TLine, TFileSize, TNameSize>(name,
                                                                        file) {
    this->platform = test_platform;
  }
  MockAsyncTraceLogger(TraceLoggingPlatform* test_platform,
                       const char name[TNameSize],
                       const char file[TFileSize],
                       TraceId id)
      : AsynchronousTraceLogger<TCategory, TLine, TFileSize, TNameSize>(name,
                                                                        id,
                                                                        file) {
    this->platform = test_platform;
  }
  MockAsyncTraceLogger(TraceLoggingPlatform* test_platform,
                       const char name[TNameSize],
                       const char file[TFileSize],
                       TraceId trace_id,
                       TraceId parent_id,
                       TraceId root_id)
      : AsynchronousTraceLogger<TCategory, TLine, TFileSize, TNameSize>(
            name,
            file,
            trace_id,
            parent_id,
            root_id) {
    this->platform = test_platform;
  }
  MockAsyncTraceLogger(TraceLoggingPlatform* test_platform,
                       const char name[TNameSize],
                       const char file[TFileSize],
                       TraceIdHierarchy ids)
      : AsynchronousTraceLogger<TCategory, TLine, TFileSize, TNameSize>(name,
                                                                        file,
                                                                        ids) {
    this->platform = test_platform;
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
                    Error));
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
                    Error));
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

// Disable unused variable warnings for UTs, since unused variables are required
// for proper functioning of trace logging. See trace_logging.h macros for
// further information.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif  // defined(__clang__)

// Test methods. Because the macros always calling TraceBase::trace_platform,
// testing will skip over the macros and directly call the relevant classes.
// Since TraceBase's IsLoggingEnabled(...) and TraceAsyncEnd(...) do no work on
// their own and only directly call the platform layer, this should not leave
// a large hole in testing.
constexpr TraceCategory::Value category =
    TraceCategory::Value::CastPlatformLayer;
constexpr uint32_t line = 10;

TEST(TraceLoggingTest, ExpectTimestampsReflectDelay) {
  constexpr uint32_t delay_in_ms = 200;
  MockLoggingPlatform mock_platform;
  TraceLoggingPlatform* platform_ptr = &mock_platform;
  EXPECT_CALL(mock_platform, LogTrace(_, _, _, _, _, _, _, _, _))
      .WillOnce(DoAll(Invoke(ValidateTraceTimestampDiff<delay_in_ms>),
                      Invoke(ValidateTraceErrorCode<Error::Code::kNone>)));

  {
    const TraceBase& TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) =
        MockSyncTraceLogger<category, line, sizeof(__FILE__), sizeof("Name")>(
            platform_ptr, "Name", __FILE__);
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
    const TraceBase& TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) =
        MockSyncTraceLogger<category, line, sizeof(__FILE__), sizeof("Name")>{
            platform_ptr, "Name", __FILE__};
    ScopedTraceOperation::SetResult({result_code});
  }
}

TEST(TraceLoggingTest, ExpectUnsetTraceIdNotSet) {
  MockLoggingPlatform mock_platform;
  TraceLoggingPlatform* platform_ptr = &mock_platform;

  TraceIdHierarchy h = {kUnsetTraceId, kUnsetTraceId, kUnsetTraceId};
  {
    const TraceBase& TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) =
        MockSyncTraceLogger<category, line, sizeof(__FILE__), sizeof("Name")>(
            platform_ptr, "Name", __FILE__, h);
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
    const TraceBase& TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) =
        MockSyncTraceLogger<category, line, sizeof(__FILE__), sizeof("Name")>(
            platform_ptr, "Name", __FILE__, h);
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
    const TraceBase& TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) =
        MockSyncTraceLogger<category, line, sizeof(__FILE__), sizeof("Name")>{
            platform_ptr, "Name", __FILE__, {current, parent, root}};
    auto ids = ScopedTraceOperation::GetHierarchy();
    EXPECT_EQ(ids.current, current);
    EXPECT_EQ(ids.parent, parent);
    EXPECT_EQ(ids.root, root);

    const TraceBase& TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) =
        MockSyncTraceLogger<category, line, sizeof(__FILE__), sizeof("Name")>{
            platform_ptr, "Name", __FILE__};
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
    const TraceBase& TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) =
        TraceIdSetter{{current, parent, root}};
    {
      const TraceBase& TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) =
          MockSyncTraceLogger<category, line, sizeof(__FILE__), sizeof("Name")>{
              platform_ptr, "Name", __FILE__};
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
    const TraceBase& TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) =
        MockSyncTraceLogger<category, line, sizeof(__FILE__), sizeof("Name")>{
            platform_ptr, "Name", __FILE__, {current, parent, root}};
    {
      const TraceBase& TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) =
          MockSyncTraceLogger<category, line, sizeof(__FILE__), sizeof("Name")>{
              platform_ptr, "Name", __FILE__};
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
    const TraceBase& TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) =
        TraceIdSetter{{current, parent, root}};
    auto ids = ScopedTraceOperation::GetHierarchy();
    EXPECT_EQ(ids.current, current);
    EXPECT_EQ(ids.parent, parent);
    EXPECT_EQ(ids.root, root);

    const TraceBase& TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) =
        MockSyncTraceLogger<category, line, sizeof(__FILE__), sizeof("Name")>{
            platform_ptr, "Name", __FILE__};
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

  {
    MockAsyncTraceLogger<category, line, sizeof(__FILE__), sizeof("Name")>{
        platform_ptr, "Name", __FILE__};
  }
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
    const TraceBase& TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) =
        TraceIdSetter{{current, parent, root}};
    {
      const TraceBase& TRACE_INTERNAL_UNIQUE_VAR_NAME(trace_ref) =
          MockAsyncTraceLogger<category, line, sizeof(__FILE__),
                               sizeof("Name")>{platform_ptr, "Name", __FILE__};
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

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif  // defined(__clang__)

}  // namespace internal
}  // namespace platform
}  // namespace openscreen
