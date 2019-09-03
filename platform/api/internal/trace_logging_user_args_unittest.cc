// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/internal/trace_logging_user_args.h"

#include "gtest/gtest.h"

namespace openscreen {
namespace platform {
namespace internal {

TEST(TraceLoggingUserArgs, BaseCase) {
  auto arg = TraceLoggingArgumentFactory::Create("name", 7);

  ASSERT_EQ(arg.name(), "name");
  ASSERT_STREQ(arg.value(), "7");
}

TEST(TraceLoggingUserArgs, StringCase) {
  auto arg = TraceLoggingArgumentFactory::Create("name", "value");

  ASSERT_EQ(arg.name(), "name");
  ASSERT_STREQ(arg.value(), "value");
}

TEST(TraceLoggingUserArgs, PointerCase) {
  int x = 7;
  auto arg = TraceLoggingArgumentFactory::Create("name", &x);

  ASSERT_EQ(arg.name(), "name");
  EXPECT_EQ(arg.value()[0], '0');
  EXPECT_EQ(arg.value()[1], 'x');
}

TEST(TraceLoggingUserArgs, NullptrCase) {
  auto arg = TraceLoggingArgumentFactory::Create("name", nullptr);

  ASSERT_EQ(arg.name(), "name");
  ASSERT_STREQ(arg.value(), "nullptr");
}

}  // namespace internal
}  // namespace platform
}  // namespace openscreen
