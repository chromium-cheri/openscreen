// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/saturate_cast.h"

#include "gtest/gtest.h"

namespace openscreen {
namespace {

TEST(SaturateCastTest, LargerToSmallerSignedInteger) {
  int64_t from = std::numeric_limits<int64_t>::max();
  EXPECT_EQ(std::numeric_limits<int32_t>::max(), saturate_cast<int32_t>(from));
  from = std::numeric_limits<int32_t>::max();
  EXPECT_EQ(std::numeric_limits<int32_t>::max(), saturate_cast<int32_t>(from));
  from = 42;
  EXPECT_EQ(42, saturate_cast<int32_t>(from));
  from = -42;
  EXPECT_EQ(-42, saturate_cast<int32_t>(from));
  from = std::numeric_limits<int32_t>::min();
  EXPECT_EQ(std::numeric_limits<int32_t>::min(), saturate_cast<int32_t>(from));
  from = std::numeric_limits<int64_t>::min();
  EXPECT_EQ(std::numeric_limits<int32_t>::min(), saturate_cast<int32_t>(from));
}

TEST(SaturateCastTest, LargerToSmallerUnsignedInteger) {
  uint64_t from = std::numeric_limits<uint64_t>::max();
  EXPECT_EQ(std::numeric_limits<uint32_t>::max(),
            saturate_cast<uint32_t>(from));
  from = std::numeric_limits<uint32_t>::max();
  EXPECT_EQ(std::numeric_limits<uint32_t>::max(),
            saturate_cast<uint32_t>(from));
  from = 42;
  EXPECT_EQ(42, saturate_cast<uint32_t>(from));
  from = std::numeric_limits<uint64_t>::min();
  EXPECT_EQ(std::numeric_limits<uint32_t>::min(),
            saturate_cast<uint32_t>(from));
}

TEST(SaturateCastTest, LargerSignedToSmallerUnsignedInteger) {
  int64_t from = std::numeric_limits<int64_t>::max();
  EXPECT_EQ(std::numeric_limits<uint32_t>::max(),
            saturate_cast<uint32_t>(from));
  from = std::numeric_limits<uint32_t>::max();
  EXPECT_EQ(std::numeric_limits<uint32_t>::max(),
            saturate_cast<uint32_t>(from));
  from = 42;
  EXPECT_EQ(42, saturate_cast<uint32_t>(from));
  from = -42;
  EXPECT_EQ(std::numeric_limits<uint32_t>::min(),
            saturate_cast<uint32_t>(from));
  from = std::numeric_limits<int64_t>::min();
  EXPECT_EQ(std::numeric_limits<uint32_t>::min(),
            saturate_cast<uint32_t>(from));
}

TEST(SaturateCastTest, LargerUnsignedToSmallerSignedInteger) {
  uint64_t from = std::numeric_limits<uint64_t>::max();
  EXPECT_EQ(std::numeric_limits<int32_t>::max(), saturate_cast<int32_t>(from));
  from = std::numeric_limits<int32_t>::max();
  EXPECT_EQ(std::numeric_limits<int32_t>::max(), saturate_cast<int32_t>(from));
  from = 42;
  EXPECT_EQ(42, saturate_cast<int32_t>(from));
  from = 0;
  EXPECT_EQ(0, saturate_cast<int32_t>(from));
}

}  // namespace
}  // namespace openscreen
