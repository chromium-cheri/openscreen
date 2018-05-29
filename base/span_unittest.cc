// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/span.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {

using SpanTest = ::testing::Test;

TEST_F(SpanTest, Constexpr) {
  constexpr int x[3] = {1, 2, 3};
  span<const int, 3> s(x);
  EXPECT_EQ(2, s[1]);
}

TEST_F(SpanTest, ModifyNonConst) {
  int x[3] = {1, 2, 3};
  span<int, dynamic_extent> s(x);
  s[1] = 4;
  EXPECT_EQ(4, x[1]);
}

TEST_F(SpanTest, RangeFor) {
  int x[3] = {1, 2, 3};
  int i = 0;
  span<int, 3> s(x);
  for (const auto& v : s) {
    EXPECT_EQ(++i, v);
  }
  EXPECT_EQ(3, i);
}

TEST_F(SpanTest, Conversion) {
  int x[3] = {2, 3, 4};
  span<int, 3> s1(x);
  span<int, dynamic_extent> s2(s1);
  EXPECT_EQ(3, s2.size());
  EXPECT_EQ(4, s2[2]);
  span<int, 3> s3(s2);
  EXPECT_EQ(3, s3.size());
  EXPECT_EQ(4, s3[2]);

  span<const int, 3> s4(s1);
  EXPECT_EQ(3, s4.size());
  EXPECT_EQ(4, s4[2]);
  span<const int, dynamic_extent> s5(s1);
  EXPECT_EQ(3, s5.size());
  EXPECT_EQ(4, s5[2]);
}

}  // namespace openscreen
