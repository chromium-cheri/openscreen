// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/integer_division.h"

#include <chrono>

#include "gtest/gtest.h"

namespace openscreen {
namespace {

constexpr int kDenominators[2] = {3, 4};

// Common test routine that tests one of the integer division functions using a
// fixed denominator and stepping, one-by-one, over a range of numerators around
// zero.
template <typename Input, typename DivideFunction>
void TestRangeAboutZero(int denom,
                        int range_of_numerators,
                        int first_expected_result,
                        DivideFunction function_to_test) {
  int expected_result = first_expected_result;
  int count_until_next_change = denom;
  for (int num = -range_of_numerators; num <= range_of_numerators; ++num) {
    EXPECT_EQ(expected_result, function_to_test(Input(num), Input(denom)))
        << "num=" << num << ", denom=" << denom;
    EXPECT_EQ(expected_result, function_to_test(Input(-num), Input(-denom)))
        << "num=" << (-num) << ", denom=" << (-denom);

    --count_until_next_change;
    if (count_until_next_change == 0) {  // Next result will be one higher.
      ++expected_result;
      count_until_next_change = denom;
    }
  }
}

TEST(IntegerDivision, DividesAndRoundsUpInts) {
  for (int denom : kDenominators) {
    TestRangeAboutZero<int>(denom, denom == 3 ? 11 : 15, -3,
                            DivideRoundingUp<int>);
  }
}

TEST(IntegerDivision, DividesAndRoundsUpChronoDurations) {
  for (int denom : kDenominators) {
    TestRangeAboutZero<std::chrono::milliseconds>(
        denom, denom == 3 ? 11 : 15, -3,
        DivideRoundingUp<std::chrono::milliseconds>);
  }
}

// Assumption: DivideRoundingUp() is working.
TEST(IntegerDivision, DividesPositivesAndRoundsUp) {
  for (int num = 0; num <= 6; ++num) {
    for (int denom = 1; denom <= 6; ++denom) {
      EXPECT_EQ(DivideRoundingUp(num, denom),
                DividePositivesRoundingUp(num, denom));
    }
  }
}

TEST(IntegerDivision, DividesAndRoundsNearestInts) {
  for (int denom : kDenominators) {
    TestRangeAboutZero<int>(denom, denom == 3 ? 10 : 14, -3,
                            DivideRoundingNearest<int>);
  }
}

TEST(IntegerDivision, DividesAndRoundsNearestChronoDurations) {
  for (int denom : kDenominators) {
    TestRangeAboutZero<std::chrono::milliseconds>(
        denom, denom == 3 ? 10 : 14, -3,
        DivideRoundingNearest<std::chrono::milliseconds>);
  }
}

// Assumption: DivideRoundingNearest() is working.
TEST(IntegerDivision, DividesPositivesAndRoundsNearest) {
  for (int num = 0; num <= 6; ++num) {
    for (int denom = 1; denom <= 6; ++denom) {
      EXPECT_EQ(DivideRoundingNearest(num, denom),
                DividePositivesRoundingNearest(num, denom));
    }
  }
}

}  // namespace
}  // namespace openscreen
