// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_pipe.h"

#include <vector>

#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {
namespace {

struct IntTraits {
  using PipeType = int;
  static constexpr int kInvalidValue = -1;

  explicit IntTraits(std::vector<int>* freed_values)
      : freed_values(freed_values) {}

  void Close(int fd) { freed_values->push_back(fd); }

  std::vector<int>* freed_values;
};

constexpr int IntTraits::kInvalidValue;

}  // namespace

TEST(ScopedPipeTest, Close) {
  std::vector<int> freed_values;
  {
    ScopedPipe<IntTraits> x(IntTraits::kInvalidValue, IntTraits(&freed_values));
  }
  ASSERT_TRUE(freed_values.empty());

  {
    ScopedPipe<IntTraits> x(3, IntTraits(&freed_values));
  }
  ASSERT_EQ(1u, freed_values.size());
  EXPECT_EQ(3, freed_values[0]);
  freed_values.clear();

  {
    ScopedPipe<IntTraits> x(3, IntTraits(&freed_values));
    EXPECT_EQ(3, x.release());

    ScopedPipe<IntTraits> y(IntTraits::kInvalidValue, IntTraits(&freed_values));
    EXPECT_EQ(IntTraits::kInvalidValue, y.release());
  }
  ASSERT_TRUE(freed_values.empty());

  {
    ScopedPipe<IntTraits> x(3, IntTraits(&freed_values));
    ScopedPipe<IntTraits> y(4, IntTraits(&freed_values));
    swap(x, y);
    EXPECT_EQ(3, y.get());
    EXPECT_EQ(4, x.get());
  }
  ASSERT_EQ(2u, freed_values.size());
  EXPECT_EQ(3, freed_values[0]);
  EXPECT_EQ(4, freed_values[1]);
  freed_values.clear();

  {
    ScopedPipe<IntTraits> x(3, IntTraits(&freed_values));
    ScopedPipe<IntTraits> y(std::move(x));
    EXPECT_EQ(IntTraits::kInvalidValue, x.get());
    EXPECT_EQ(3, y.get());
    EXPECT_TRUE(freed_values.empty());
  }
  ASSERT_EQ(1u, freed_values.size());
  EXPECT_EQ(3, freed_values[0]);
  freed_values.clear();

  {
    ScopedPipe<IntTraits> x(3, IntTraits(&freed_values));
    ScopedPipe<IntTraits> y(4, IntTraits(&freed_values));
    y = std::move(x);
    EXPECT_EQ(IntTraits::kInvalidValue, x.get());
    EXPECT_EQ(3, y.get());
    EXPECT_EQ(1u, freed_values.size());
    EXPECT_EQ(4, freed_values[0]);
  }
  ASSERT_EQ(2u, freed_values.size());
  EXPECT_EQ(4, freed_values[0]);
  EXPECT_EQ(3, freed_values[1]);
  freed_values.clear();
}

TEST(ScopedPipeTest, Comparisons) {
  std::vector<int> freed_values;
  ScopedPipe<IntTraits> x(IntTraits::kInvalidValue, IntTraits(&freed_values));
  ScopedPipe<IntTraits> y(IntTraits::kInvalidValue, IntTraits(&freed_values));
  EXPECT_FALSE(x);
  EXPECT_EQ(x, y);

  x = ScopedPipe<IntTraits>(3, IntTraits(&freed_values));
  EXPECT_TRUE(x);
  EXPECT_NE(x, y);

  y = ScopedPipe<IntTraits>(4, IntTraits(&freed_values));
  EXPECT_TRUE(y);
  EXPECT_NE(x, y);

  y = ScopedPipe<IntTraits>(3, IntTraits(&freed_values));
  EXPECT_EQ(x, y);
}

}  // namespace openscreen
