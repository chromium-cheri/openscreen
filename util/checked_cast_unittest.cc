// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/checked_cast.h"

#include <math.h>

#include <cstdint>

#include "gtest/gtest.h"
#include "util/osp_logging.h"

static_assert(std::numeric_limits<float>::has_quiet_NaN,
              "quiet_NaN not defined for float");
static_assert(std::numeric_limits<double>::has_quiet_NaN,
              "quiet_NAN not defined for double");

namespace openscreen {

TEST(CheckedCastTest, ValidCasts) {
  // uint32_t => uint8_t
  EXPECT_EQ(static_cast<uint8_t>(0), checked_cast<uint8_t>(0U));
  EXPECT_EQ(static_cast<uint8_t>(255), checked_cast<uint8_t>(255U));
  // uint32_t => int8_t
  EXPECT_EQ(static_cast<int8_t>(0), checked_cast<int8_t>(0U));
  EXPECT_EQ(static_cast<int8_t>(127), checked_cast<int8_t>(127U));
  // int32_t => uint8_t
  EXPECT_EQ(static_cast<uint8_t>(0), checked_cast<uint8_t>(0));
  EXPECT_EQ(static_cast<uint8_t>(255), checked_cast<uint8_t>(255));
  // int32_t => int8_t
  EXPECT_EQ(static_cast<int8_t>(-128), checked_cast<int8_t>(-128));
  EXPECT_EQ(static_cast<int8_t>(127), checked_cast<int8_t>(127));

  // uint64_t => uint32_t
  OSP_LOG_INFO << checked_cast<uint32_t>(0UL);
  EXPECT_EQ(0U, checked_cast<uint32_t>(0UL));
  EXPECT_EQ(4294967295U, checked_cast<uint32_t>(4294967295UL));
  // uint64_t => int32_t
  EXPECT_EQ(0, checked_cast<int32_t>(0UL));
  EXPECT_EQ(2147483647, checked_cast<int32_t>(2147483647UL));
  // int64_t => uint32_t
  OSP_LOG_INFO << checked_cast<uint32_t>(0UL);
  EXPECT_EQ(0U, checked_cast<uint32_t>(0UL));
  OSP_LOG_INFO << checked_cast<uint32_t>(4294967295UL);
  EXPECT_EQ(4294967295U, checked_cast<uint32_t>(4294967295UL));
  // int64_t => int32_t
  OSP_LOG_INFO << checked_cast<int32_t>(-2147483648UL);
  EXPECT_EQ(-2147483648, checked_cast<int32_t>(-2147483648UL));
  OSP_LOG_INFO << checked_cast<int32_t>(2147483647UL);
  EXPECT_EQ(2147483647, checked_cast<int32_t>(2147483647UL));

  // double => float
  EXPECT_EQ(3.402823466e+38f, checked_cast<float>(3.402823466e+38));
  EXPECT_EQ(-3.402823466e+38f, checked_cast<float>(-3.402823466e+38));
  EXPECT_EQ(std::numeric_limits<float>::quiet_NaN(),
            checked_cast<float>(std::numeric_limits<double>::quiet_NaN()));
}

}  // namespace openscreen
