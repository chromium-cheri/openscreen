// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/string_span.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {

using StringSpanTest = ::testing::Test;

TEST_F(StringSpanTest, ModifyNonConst) {
  std::string x("asdfqwer");
  string_span<> s(x);
  s[3] = '1';
  EXPECT_EQ('1', x[3]);
}

TEST_F(StringSpanTest, RangeFor) {
  std::string x("asdfqwer");
  cstring_span<> s(x);
  int i = 0;
  for (const auto& c : s) {
    EXPECT_EQ(x[i], c);
    ++i;
  }
}

TEST_F(StringSpanTest, RemoveZ) {
  cstring_span<> s("asdf");
  ASSERT_EQ(4, s.size());
  EXPECT_EQ('f', s[3]);
}

TEST_F(StringSpanTest, Data) {
  cstring_span<> s1("asdf");
  for (int i = 0; i < s1.size(); ++i) {
    EXPECT_EQ(s1[i], s1.data()[i]);
  }
  EXPECT_EQ(0, s1.data()[s1.size()]);

  std::string x("qweroiuzlxkj");
  cstring_span<> s2(x);
  for (int i = 0; i < s2.size(); ++i) {
    EXPECT_EQ(x[i], s2.data()[i]);
  }
  EXPECT_EQ(0, s2.data()[s2.size()]);
}

}  // namespace openscreen
