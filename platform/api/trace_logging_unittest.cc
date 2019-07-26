// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/trace_logging.h"

#include <chrono>
#include <thread>

#include "absl/types/optional.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/test/trace_logging_helpers.h"

namespace openscreen {
namespace platform {
namespace {
constexpr TraceHierarchyParts kAllParts = static_cast<TraceHierarchyParts>(
    TraceHierarchyParts::kRoot | TraceHierarchyParts::kParent |
    TraceHierarchyParts::kCurrent);
constexpr TraceHierarchyParts kParentAndRoot = static_cast<TraceHierarchyParts>(
    TraceHierarchyParts::kRoot | TraceHierarchyParts::kParent);
constexpr TraceId kEmptyId = TraceId{0};
}  // namespace

using ::testing::_;
using ::testing::Invoke;
using namespace std::placeholders;

void ValidateUserArgument(const char* name,
                          const uint32_t line,
                          const char* file,
                          Clock::time_point start_time,
                          Clock::time_point end_time,
                          TraceIdHierarchy ids,
                          Error error,
                          absl::optional<UserDefinedArgument> arg1,
                          absl::optional<UserDefinedArgument> arg2,
                          ArgumentId which_arg,
                          const char* arg_name,
                          const char* arg_value) {
  absl::optional<UserDefinedArgument>* arg =
      which_arg == ArgumentId::kFirst ? &arg1 : &arg2;

  EXPECT_TRUE(arg->has_value());
  ASSERT_STREQ(arg_name, arg->value().name);
  ASSERT_STREQ(arg_value, arg->value().value);
}

std::function<void(const char*,
                   const uint32_t,
                   const char*,
                   Clock::time_point,
                   Clock::time_point,
                   TraceIdHierarchy,
                   Error,
                   absl::optional<UserDefinedArgument>,
                   absl::optional<UserDefinedArgument>)>
GetCheckUserArgFunc(ArgumentId which_arg, const char* name, const char* value) {
  return std::bind(ValidateUserArgument, _1, _2, _3, _4, _5, _6, _7, _8, _9,
                   which_arg, name, value);
}

TEST(TraceLoggingTest, MacroCallScopedDoesntSegFault) {
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, _, _, _, _)).Times(1);
  { TRACE_SCOPED(TraceCategory::Value::Any, "test"); }
}

TEST(TraceLoggingTest, MacroCallUnscopedDoesntSegFault) {
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogAsyncStart(_, _, _, _, _, _, _)).Times(1);
  { TRACE_ASYNC_START(TraceCategory::Value::Any, "test"); }
}

TEST(TraceLoggingTest, MacroVariablesUniquelyNames) {
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, _, _, _, _)).Times(2);
  EXPECT_CALL(platform, LogAsyncStart(_, _, _, _, _, _, _)).Times(2);

  {
    TRACE_SCOPED(TraceCategory::Value::Any, "test1");
    TRACE_SCOPED(TraceCategory::Value::Any, "test2");
    TRACE_ASYNC_START(TraceCategory::Value::Any, "test3");
    TRACE_ASYNC_START(TraceCategory::Value::Any, "test4");
  }
}

TEST(TraceLoggingTest, ExpectTimestampsReflectDelay) {
  constexpr uint32_t delay_in_ms = 50;
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, _, _, _, _))
      .WillOnce(DoAll(Invoke(ValidateTraceTimestampDiff<delay_in_ms>),
                      Invoke(ValidateTraceErrorCode<Error::Code::kNone>)));

  {
    TRACE_SCOPED(TraceCategory::Value::Any, "Name");
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_in_ms));
  }
}

TEST(TraceLoggingTest, ExpectErrorsPassedToResult) {
  constexpr Error::Code result_code = Error::Code::kParseError;
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, _, _, _, _))
      .WillOnce(Invoke(ValidateTraceErrorCode<result_code>));

  {
    TRACE_SCOPED(TraceCategory::Value::Any, "Name");
    TRACE_SET_RESULT(result_code);
  }
}

TEST(TraceLoggingTest, ExpectUnsetTraceIdNotSet) {
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, _, _, _, _)).Times(1);

  TraceIdHierarchy h = {kUnsetTraceId, kUnsetTraceId, kUnsetTraceId};
  {
    TRACE_SCOPED(TraceCategory::Value::Any, "Name", h);

    auto ids = TRACE_HIERARCHY;
    EXPECT_NE(ids.current, kUnsetTraceId);
    EXPECT_NE(ids.parent, kUnsetTraceId);
    EXPECT_NE(ids.root, kUnsetTraceId);
  }
}

TEST(TraceLoggingTest, ExpectCreationWithIdsToWork) {
  constexpr TraceId current = 0x32;
  constexpr TraceId parent = 0x47;
  constexpr TraceId root = 0x84;
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, _, _, _, _))
      .WillOnce(
          DoAll(Invoke(ValidateTraceErrorCode<Error::Code::kNone>),
                Invoke(ValidateTraceIdHierarchyOnSyncTrace<current, parent,
                                                           root, kAllParts>)));

  {
    TraceIdHierarchy h = {current, parent, root};
    TRACE_SCOPED(TraceCategory::Value::Any, "Name", h);

    auto ids = TRACE_HIERARCHY;
    EXPECT_EQ(ids.current, current);
    EXPECT_EQ(ids.parent, parent);
    EXPECT_EQ(ids.root, root);

    EXPECT_EQ(TRACE_CURRENT_ID, current);
    EXPECT_EQ(TRACE_ROOT_ID, root);
  }
}

TEST(TraceLoggingTest, ExpectHirearchyToBeApplied) {
  constexpr TraceId current = 0x32;
  constexpr TraceId parent = 0x47;
  constexpr TraceId root = 0x84;
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, _, _, _, _))
      .WillOnce(DoAll(
          Invoke(ValidateTraceErrorCode<Error::Code::kNone>),
          Invoke(ValidateTraceIdHierarchyOnSyncTrace<kEmptyId, current, root,
                                                     kParentAndRoot>)))
      .WillOnce(
          DoAll(Invoke(ValidateTraceErrorCode<Error::Code::kNone>),
                Invoke(ValidateTraceIdHierarchyOnSyncTrace<current, parent,
                                                           root, kAllParts>)));

  {
    TraceIdHierarchy h = {current, parent, root};
    TRACE_SCOPED(TraceCategory::Value::Any, "Name", h);
    auto ids = TRACE_HIERARCHY;
    EXPECT_EQ(ids.current, current);
    EXPECT_EQ(ids.parent, parent);
    EXPECT_EQ(ids.root, root);

    TRACE_SCOPED(TraceCategory::Value::Any, "Name");
    ids = TRACE_HIERARCHY;
    EXPECT_NE(ids.current, current);
    EXPECT_EQ(ids.parent, current);
    EXPECT_EQ(ids.root, root);
  }
}

TEST(TraceLoggingTest, ExpectHirearchyToEndAfterScopeWhenSetWithSetter) {
  constexpr TraceId current = 0x32;
  constexpr TraceId parent = 0x47;
  constexpr TraceId root = 0x84;
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, _, _, _, _))
      .WillOnce(DoAll(
          Invoke(ValidateTraceErrorCode<Error::Code::kNone>),
          Invoke(ValidateTraceIdHierarchyOnSyncTrace<kEmptyId, current, root,
                                                     kParentAndRoot>)));

  {
    TraceIdHierarchy ids = {current, parent, root};
    TRACE_SET_HIERARCHY(ids);
    {
      TRACE_SCOPED(TraceCategory::Value::Any, "Name");
      ids = TRACE_HIERARCHY;
      EXPECT_NE(ids.current, current);
      EXPECT_EQ(ids.parent, current);
      EXPECT_EQ(ids.root, root);
    }

    ids = TRACE_HIERARCHY;
    EXPECT_EQ(ids.current, current);
    EXPECT_EQ(ids.parent, parent);
    EXPECT_EQ(ids.root, root);
  }
}

TEST(TraceLoggingTest, ExpectHirearchyToEndAfterScope) {
  constexpr TraceId current = 0x32;
  constexpr TraceId parent = 0x47;
  constexpr TraceId root = 0x84;
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, _, _, _, _))
      .WillOnce(DoAll(
          Invoke(ValidateTraceErrorCode<Error::Code::kNone>),
          Invoke(ValidateTraceIdHierarchyOnSyncTrace<kEmptyId, current, root,
                                                     kParentAndRoot>)))
      .WillOnce(
          DoAll(Invoke(ValidateTraceErrorCode<Error::Code::kNone>),
                Invoke(ValidateTraceIdHierarchyOnSyncTrace<current, parent,
                                                           root, kAllParts>)));

  {
    TraceIdHierarchy ids = {current, parent, root};
    TRACE_SCOPED(TraceCategory::Value::Any, "Name", ids);
    {
      TRACE_SCOPED(TraceCategory::Value::Any, "Name");
      ids = TRACE_HIERARCHY;
      EXPECT_NE(ids.current, current);
      EXPECT_EQ(ids.parent, current);
      EXPECT_EQ(ids.root, root);
    }

    ids = TRACE_HIERARCHY;
    EXPECT_EQ(ids.current, current);
    EXPECT_EQ(ids.parent, parent);
    EXPECT_EQ(ids.root, root);
  }
}

TEST(TraceLoggingTest, ExpectSetHierarchyToApply) {
  constexpr TraceId current = 0x32;
  constexpr TraceId parent = 0x47;
  constexpr TraceId root = 0x84;
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, _, _, _, _))
      .WillOnce(DoAll(
          Invoke(ValidateTraceErrorCode<Error::Code::kNone>),
          Invoke(ValidateTraceIdHierarchyOnSyncTrace<kEmptyId, current, root,
                                                     kParentAndRoot>)));

  {
    TraceIdHierarchy ids = {current, parent, root};
    TRACE_SET_HIERARCHY(ids);
    ids = TRACE_HIERARCHY;
    EXPECT_EQ(ids.current, current);
    EXPECT_EQ(ids.parent, parent);
    EXPECT_EQ(ids.root, root);

    TRACE_SCOPED(TraceCategory::Value::Any, "Name");
    ids = TRACE_HIERARCHY;
    EXPECT_NE(ids.current, current);
    EXPECT_EQ(ids.parent, current);
    EXPECT_EQ(ids.root, root);
  }
}

TEST(TraceLoggingTest, ExpectUserArgsNotPresentWhenNotProvided) {
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, _, _, _, _))
      .WillOnce(DoAll(Invoke(ValidateUserArgumentEmpty<ArgumentId::kFirst>),
                      Invoke(ValidateUserArgumentEmpty<ArgumentId::kSecond>)));

  { TRACE_SCOPED(TraceCategory::Value::Any, "Name"); }
}

TEST(TraceLoggingTest, ExpectUserArgsValidString) {
  const char* arg_name = "string";
  const char* arg_value = "value";
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, _, _, _, _))
      .WillOnce(DoAll(
          Invoke(GetCheckUserArgFunc(ArgumentId::kFirst, arg_name, arg_value)),
          Invoke(ValidateUserArgumentEmpty<ArgumentId::kSecond>)));
  { TRACE_SCOPED(TraceCategory::Value::Any, "Name", arg_name, arg_value); }
}

TEST(TraceLoggingTest, ExpectUserArgsValidInt) {
  constexpr const char* arg_name = "integer";
  constexpr const char* arg_value = "1";
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, _, _, _, _))
      .WillOnce(DoAll(
          Invoke(GetCheckUserArgFunc(ArgumentId::kFirst, arg_name, arg_value)),
          Invoke(ValidateUserArgumentEmpty<ArgumentId::kSecond>)));
  { TRACE_SCOPED(TraceCategory::Value::Any, "Name", arg_name, 1); }
}

TEST(TraceLoggingTest, ExpectUserArgsValidFunction) {
  constexpr const char* arg_name = "function";
  constexpr const char* arg_value = "3";
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, _, _, _, _))
      .WillOnce(DoAll(
          Invoke(GetCheckUserArgFunc(ArgumentId::kFirst, arg_name, arg_value)),
          Invoke(ValidateUserArgumentEmpty<ArgumentId::kSecond>)));
  {
    int value = 1;
    std::function<int()> func = [&value]() { return value; };
    TRACE_SCOPED(TraceCategory::Value::Any, "Name", arg_name, func);
    value += 2;
  }
}

TEST(TraceLoggingTest, ExpectBothUserArgsValid) {
  constexpr const char* arg_name = "string";
  constexpr const char* arg_value = "value";
  constexpr const char* arg_name2 = "string2";
  constexpr const char* arg_value2 = "value2";
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogTrace(_, _, _, _, _, _, _, _, _))
      .WillOnce(DoAll(
          Invoke(GetCheckUserArgFunc(ArgumentId::kFirst, arg_name, arg_value)),
          Invoke(GetCheckUserArgFunc(ArgumentId::kSecond, arg_name2,
                                     arg_value2))));
  {
    TRACE_SCOPED(TraceCategory::Value::Any, "Name", arg_name, arg_value,
                 arg_name2, arg_value2);
  }
}

TEST(TraceLoggingTest, CheckTraceAsyncStartLogsCorrectly) {
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogAsyncStart(_, _, _, _, _, _, _)).Times(1);

  { TRACE_ASYNC_START(TraceCategory::Value::Any, "Name"); }
}

TEST(TraceLoggingTest, CheckTraceAsyncStartSetsHierarchy) {
  constexpr TraceId current = 32;
  constexpr TraceId parent = 47;
  constexpr TraceId root = 84;
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogAsyncStart(_, _, _, _, _, _, _))
      .WillOnce(
          Invoke(ValidateTraceIdHierarchyOnAsyncTrace<kEmptyId, current, root,
                                                      kParentAndRoot>));

  {
    TraceIdHierarchy ids = {current, parent, root};
    TRACE_SET_HIERARCHY(ids);
    {
      TRACE_ASYNC_START(TraceCategory::Value::Any, "Name");
      ids = TRACE_HIERARCHY;
      EXPECT_NE(ids.current, current);
      EXPECT_EQ(ids.parent, current);
      EXPECT_EQ(ids.root, root);
    }

    ids = TRACE_HIERARCHY;
    EXPECT_EQ(ids.current, current);
    EXPECT_EQ(ids.parent, parent);
    EXPECT_EQ(ids.root, root);
  }
}

TEST(TraceLoggingTest, CheckTraceAsyncEndLogsCorrectly) {
  constexpr TraceId id = 12345;
  constexpr Error::Code result = Error::Code::kAgain;
  MockLoggingPlatform platform;
  TRACE_SET_DEFAULT_PLATFORM(&platform);
  EXPECT_CALL(platform, LogAsyncEnd(_, _, _, id, result)).Times(1);

  TRACE_ASYNC_END(TraceCategory::Value::Any, id, result);
}

}  // namespace platform
}  // namespace openscreen
