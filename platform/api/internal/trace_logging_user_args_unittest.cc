// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/internal/trace_logging_user_args.h"

#include "gtest/gtest.h"

namespace openscreen {
namespace platform {
namespace internal {

class TraceLoggingArgumentFactory {
 public:
  template <typename T>
  static TraceLoggingArgument<T> Create(const char* name, T value) {
    return TraceLoggingArgument<T>(name, value);
  }
};

TEST(TraceLoggingUserArgs, StringCase) {
  auto arg = TraceLoggingArgumentFactory::Create("name", "value");

  ASSERT_EQ(arg.name(), "name");
  ASSERT_EQ(arg.value().type(), UserArgumentValue::DataType::kString);
  ASSERT_STREQ(arg.value().data().string, "value");
}

TEST(TraceLoggingUserArgs, PointerCase) {
  int x = 7;
  auto arg = TraceLoggingArgumentFactory::Create("name", &x);

  ASSERT_EQ(arg.name(), "name");
  ASSERT_EQ(arg.value().type(), UserArgumentValue::DataType::kString);
  EXPECT_EQ(arg.value().data().string[0], '0');
  EXPECT_EQ(arg.value().data().string[1], 'x');
}

TEST(TraceLoggingUserArgs, NullptrCase) {
  auto arg = TraceLoggingArgumentFactory::Create("name", nullptr);

  ASSERT_EQ(arg.name(), "name");
  ASSERT_EQ(arg.value().type(), UserArgumentValue::DataType::kString);
  ASSERT_STREQ(arg.value().data().string, "nullptr");
}

TEST(TraceLoggingUserArgs, IntegerCase) {
  auto arg = TraceLoggingArgumentFactory::Create("name", int64_t{7});

  ASSERT_EQ(arg.name(), "name");
  ASSERT_EQ(arg.value().type(), UserArgumentValue::DataType::kInteger);
  ASSERT_EQ(arg.value().data().integer, int64_t{7});
}

TEST(TraceLoggingUserArgs, DoubleCase) {
  auto arg = TraceLoggingArgumentFactory::Create("name", 7.5);

  ASSERT_EQ(arg.name(), "name");
  ASSERT_EQ(arg.value().type(), UserArgumentValue::DataType::kFloatingPoint);
  ASSERT_EQ(arg.value().data().floating_point, 7.5);
}

}  // namespace internal
}  // namespace platform
}  // namespace openscreen
