// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/internal/trace_logging_creation_helper.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/base/error.h"

namespace openscreen {
namespace platform {
namespace internal {

using ::testing::_;
using ::testing::Invoke;

TEST(TraceLoggingCreationHelper, CreateConstCharArg) {
  const TraceLoggingArgument& arg =
      TraceLoggingArgumentFactory::Create("name", "value");
  ASSERT_STREQ(arg.name(), "name");
  ASSERT_STREQ(arg.value(), "value");
}

TEST(TraceLoggingCreationHelper, CreateStringStreamArg) {
  const TraceLoggingArgument& arg =
      TraceLoggingArgumentFactory::Create("name", 1);
  ASSERT_STREQ(arg.name(), "name");
  ASSERT_STREQ(arg.value(), "1");

  const TraceLoggingArgument& arg2 =
      TraceLoggingArgumentFactory::Create("name", Error::Code::kNone);
  ASSERT_STREQ(arg2.name(), "name");
  ASSERT_STREQ(arg2.value(), "Success");
}

TEST(TraceLoggingCreationHelper, CreateFunctionArg) {
  int value = 0;
  std::function<int()> func = [value_ptr = &value]() { return (*value_ptr)++; };
  const TraceLoggingArgument& arg =
      TraceLoggingArgumentFactory::Create("name", func);
  ASSERT_STREQ(arg.name(), "name");
  EXPECT_EQ(value, 0);
  ASSERT_STREQ(arg.value(), "0");
  EXPECT_EQ(value, 1);
  ASSERT_STREQ(arg.value(), "0");
  EXPECT_EQ(value, 1);

  const TraceLoggingArgument& arg2 =
      TraceLoggingArgumentFactory::Create("name2", func);
  ASSERT_STREQ(arg2.name(), "name2");
  EXPECT_EQ(value, 1);
  ASSERT_STREQ(arg2.value(), "1");
  EXPECT_EQ(value, 2);
  ASSERT_STREQ(arg2.value(), "1");
  EXPECT_EQ(value, 2);
}

}  // namespace internal
}  // namespace platform
}  // namespace openscreen
