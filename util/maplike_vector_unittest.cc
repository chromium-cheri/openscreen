// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/maplike_vector.h"

#include <chrono>

#include "gtest/gtest.h"

namespace openscreen {

namespace {

const MaplikeVector<int, const char*> kSimpleMaplikeVector{{-1, "bar"},
                                                           {123, "foo"},
                                                           {10000, "baz"},
                                                           {0, ""}};

}  // namespace

TEST(MaplikeVectorTest, Contains) {
  EXPECT_TRUE(kSimpleMaplikeVector.Contains(-1));
  EXPECT_TRUE(kSimpleMaplikeVector.Contains(123));
  EXPECT_TRUE(kSimpleMaplikeVector.Contains(10000));
  EXPECT_TRUE(kSimpleMaplikeVector.Contains(0));
  EXPECT_FALSE(kSimpleMaplikeVector.Contains(2));
}

TEST(MaplikeVectorTest, ContainsValue) {
  EXPECT_TRUE(kSimpleMaplikeVector.ContainsValue("bar"));
  EXPECT_TRUE(kSimpleMaplikeVector.ContainsValue("foo"));
  EXPECT_TRUE(kSimpleMaplikeVector.ContainsValue("baz"));
  EXPECT_TRUE(kSimpleMaplikeVector.ContainsValue(""));
  EXPECT_FALSE(kSimpleMaplikeVector.ContainsValue("buzz"));
}

TEST(MaplikeVectorTest, OperatorAccessor) {
  EXPECT_STREQ("bar", kSimpleMaplikeVector[-1]);
  EXPECT_STREQ("foo", kSimpleMaplikeVector[123]);
  EXPECT_STREQ("baz", kSimpleMaplikeVector[10000]);
  EXPECT_STREQ("", kSimpleMaplikeVector[0]);

  EXPECT_DEATH(kSimpleMaplikeVector[31337], ".*Key not in maplike vector");
}

TEST(MaplikeVectorTest, Get) {
  EXPECT_STREQ("bar", kSimpleMaplikeVector.Get(-1).value());
  EXPECT_STREQ("foo", kSimpleMaplikeVector.Get(123).value());
  EXPECT_STREQ("baz", kSimpleMaplikeVector.Get(10000).value());
  EXPECT_STREQ("", kSimpleMaplikeVector.Get(0).value());

  EXPECT_FALSE(kSimpleMaplikeVector.Get(31337));
}

TEST(MaplikeVectorTest, RemovalAndAddition) {
  auto mutable_vector = kSimpleMaplikeVector;
  EXPECT_TRUE(mutable_vector.Contains(-1));
  EXPECT_TRUE(mutable_vector.Remove(-1));
  EXPECT_FALSE(mutable_vector.Contains(-1));

  // We shouldn't fail if we remove something that's not there.
  EXPECT_FALSE(mutable_vector.Remove(123456));

  mutable_vector.Insert(-1, "bar");
  EXPECT_TRUE(mutable_vector.Contains(-1));
}

#if OSP_DCHECK_IS_ON()

TEST(MaplikeVectorTest, ChecksIfTooBig) {
  EXPECT_DEATH((MaplikeVector<int, int>(33)),
               ".*With this many elements you probably want a map instead");

  MaplikeVector<int, int> vec;
  for (std::size_t i = 0; i < MaplikeVector<int, int>::kMaxSize; ++i) {
    vec.Insert(i, i << 1);
  }
  EXPECT_DEATH(vec.Insert(-1, -2),
               ".*With this many elements you probably want a map instead");
}

#endif  // OSP_DCHECK_IS_ON()

TEST(MaplikeVectorTest, Mutation) {
  MaplikeVector<int, int> mutable_vector{{1, 2}};
  EXPECT_EQ(2, mutable_vector[1]);

  mutable_vector[1] = 3;
  EXPECT_EQ(3, mutable_vector[1]);
}
}  // namespace openscreen
