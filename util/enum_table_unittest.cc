// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/enum_table.h"

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace {

enum class MyEnum { kZero, kOne, kTwo, kMaxValue = kTwo };

const EnumTable<MyEnum> kSorted({{MyEnum::kZero, "ZERO"},
                                 {MyEnum::kOne, "ONE"},
                                 {MyEnum::kTwo, "TWO"}},
                                MyEnum::kMaxValue);

const EnumTable<MyEnum> kUnsorted({{MyEnum::kOne, "ONE"},
                                   {MyEnum::kZero, "ZERO"},
                                   {MyEnum::kTwo, "TWO"}},
                                  NonConsecutiveEnumTable);

const EnumTable<MyEnum> kSortedMissing({{MyEnum::kZero, "ZERO"},
                                        {MyEnum::kOne},
                                        {MyEnum::kTwo, "TWO"}},
                                       MyEnum::kMaxValue);

const EnumTable<MyEnum> kUnsortedMissing({{MyEnum::kOne, "ONE"},
                                          {MyEnum::kZero},
                                          {MyEnum::kTwo, "TWO"}},
                                         NonConsecutiveEnumTable);

// absl::string_view to const char comparisons can be messy on
// linux TSAN builds.
void ExpectStringviewEq(absl::string_view expected,
                        absl::optional<absl::string_view> actual) {
  EXPECT_TRUE(actual.has_value());
  // we have a failure if this is false already, but no need to ASSERT or
  // crash the test.
  if (actual.has_value()) {
    EXPECT_TRUE(expected == actual.value());
  }
}

}  // namespace

template <>
const EnumTable<MyEnum> EnumTable<MyEnum>::instance(
    {{MyEnum::kZero, "ZERO_DEFAULT"},
     {MyEnum::kOne, "ONE_DEFAULT"},
     {MyEnum::kTwo, "TWO_DEFAULT"}},
    MyEnum::kMaxValue);

namespace {

TEST(EnumTableTest, TestGetString) {
  const auto first = kSorted.GetString(MyEnum::kZero);
  ExpectStringviewEq("ZERO", first);
  ExpectStringviewEq("ZERO", kSorted.GetString(MyEnum::kZero));
  ExpectStringviewEq("ONE", kSorted.GetString(MyEnum::kOne));
  ExpectStringviewEq("TWO", kSorted.GetString(MyEnum::kTwo));
}

TEST(EnumTableTest, TestGetStringUnsorted) {
  ExpectStringviewEq("ZERO", kUnsorted.GetString(MyEnum::kZero));
  ExpectStringviewEq("ONE", kUnsorted.GetString(MyEnum::kOne));
  ExpectStringviewEq("TWO", kUnsorted.GetString(MyEnum::kTwo));
}

TEST(EnumTableTest, TestGetMissingString) {
  ExpectStringviewEq("ZERO", kSortedMissing.GetString(MyEnum::kZero));
  EXPECT_EQ(absl::nullopt, kSortedMissing.GetString(MyEnum::kOne));
  ExpectStringviewEq("TWO", kSortedMissing.GetString(MyEnum::kTwo));
}

TEST(EnumTableTest, TestGetMissingStringUnsorted) {
  EXPECT_EQ(absl::nullopt, kUnsortedMissing.GetString(MyEnum::kZero));
  ExpectStringviewEq("ONE", kUnsortedMissing.GetString(MyEnum::kOne));
  ExpectStringviewEq("TWO", kUnsortedMissing.GetString(MyEnum::kTwo));
}

TEST(EnumTableTest, TestEnumToStringGlobal) {
  ExpectStringviewEq("ZERO_DEFAULT", EnumToString(MyEnum::kZero));
  ExpectStringviewEq("ONE_DEFAULT", EnumToString(MyEnum::kOne));
  ExpectStringviewEq("TWO_DEFAULT", EnumToString(MyEnum::kTwo));
}

TEST(EnumTableTest, TestStaticGetString) {
  ExpectStringviewEq("ZERO", kSorted.GetString<MyEnum::kZero>());
  ExpectStringviewEq("ONE", kSorted.GetString<MyEnum::kOne>());
  ExpectStringviewEq("TWO", kSorted.GetString<MyEnum::kTwo>());
}

TEST(EnumTableTest, TestStaticEnumToStringGlobal) {
  // Extra parens are needed below, otherwise Clang gets confused.
  ExpectStringviewEq("ZERO_DEFAULT", (EnumToString<MyEnum, MyEnum::kZero>()));
  ExpectStringviewEq("ONE_DEFAULT", (EnumToString<MyEnum, MyEnum::kOne>()));
  ExpectStringviewEq("TWO_DEFAULT", (EnumToString<MyEnum, MyEnum::kTwo>()));
}

TEST(EnumTableTest, TestGetEnum) {
  EXPECT_EQ(MyEnum::kZero, kSorted.GetEnum("ZERO"));
  EXPECT_EQ(MyEnum::kOne, kSorted.GetEnum("ONE"));
  EXPECT_EQ(MyEnum::kTwo, kSorted.GetEnum("TWO"));
  EXPECT_EQ(absl::nullopt, kSorted.GetEnum("THREE"));
  EXPECT_EQ(absl::nullopt, kSorted.GetEnum(""));
}

TEST(EnumTableTest, TestStringToEnumGlobal) {
  EXPECT_EQ(MyEnum::kZero, StringToEnum<MyEnum>("ZERO_DEFAULT"));
  EXPECT_EQ(MyEnum::kOne, StringToEnum<MyEnum>("ONE_DEFAULT"));
  EXPECT_EQ(MyEnum::kTwo, StringToEnum<MyEnum>("TWO_DEFAULT"));
  EXPECT_EQ(absl::nullopt, StringToEnum<MyEnum>("THREE"));
  EXPECT_EQ(absl::nullopt, StringToEnum<MyEnum>(""));
}

// See note in enum_table.h for details of why these tests have to be compiled
// out when NDEBUG is defined.
#ifndef NDEBUG

TEST(EnumTableDeathTest, MaxValueTooSmall) {
  EXPECT_DEATH(EnumTable<MyEnum>({{MyEnum::kZero, "ZERO"},
                                  {MyEnum::kOne, "ONE"},
                                  {MyEnum::kTwo, "TWO"}},
                                 MyEnum::kOne),
               ".*Missing entry for enum value 1");
}

TEST(EnumTableDeathTest, MaxValueTooLarge) {
  EXPECT_DEATH(
      EnumTable<MyEnum>({{MyEnum::kZero, "ZERO"}, {MyEnum::kOne, "ONE"}},
                        MyEnum::kTwo),
      ".*Missing entry for enum value 2");
}

TEST(EnumTableDeathTest, Sorted) {
  EXPECT_DEATH(EnumTable<MyEnum>({{MyEnum::kZero, "ZERO"},
                                  {MyEnum::kTwo, "TWO"},
                                  {MyEnum::kOne, "ONE"}},
                                 MyEnum::kMaxValue),
               ".*numerical values must be consecutive integers.*");
}

TEST(EnumTableDeathTest, Unsorted) {
  EXPECT_DEATH(EnumTable<MyEnum>({{MyEnum::kZero, "ZERO"},
                                  {MyEnum::kOne, "ONE"},
                                  {MyEnum::kTwo, "TWO"}},
                                 NonConsecutiveEnumTable),
               ".*Don't use this constructor for sorted entries.*");
}

TEST(EnumTableDeathTest, DuplicateEnums) {
  EXPECT_DEATH(EnumTable<MyEnum>({{MyEnum::kZero, "ZERO"},
                                  {MyEnum::kTwo, "TWO"},
                                  {MyEnum::kOne, "ONE"},
                                  {MyEnum::kZero, "ZERO"}},
                                 NonConsecutiveEnumTable),
               ".*Found duplicate enum values*");
}

TEST(EnumTableDeathTest, DuplicateStrings) {
  EXPECT_DEATH(EnumTable<MyEnum>({{MyEnum::kZero, "FOO"},
                                  {MyEnum::kOne, "ONE"},
                                  {MyEnum::kTwo, "FOO"}},
                                 MyEnum::kMaxValue),
               ".*Found duplicate strings.*");
}

constexpr MyEnum kInvalid =
    static_cast<MyEnum>(static_cast<int>(MyEnum::kMaxValue) + 1);

TEST(EnumTableDeathTest, EnumToString) {
  EXPECT_DEATH(kSorted.GetString<kInvalid>(), ".*No string for enum value.*");
}

TEST(EnumTableDeathTest, StaticEnumToString) {
  EXPECT_DEATH((EnumToString<MyEnum, kInvalid>()), ".*");
}

enum class HugeEnum {
  k0,
  k1,
  k2,
  k3,
  k4,
  k5,
  k6,
  k7,
  k8,
  k9,
  k10,
  k11,
  k12,
  k13,
  k14,
  k15,
  k16,
  k17,
  k18,
  k19,
  k20,
  k21,
  k22,
  k23,
  k24,
  k25,
  k26,
  k27,
  k28,
  k29,
  k30,
  k31,
  k32,
  kMaxValue = k32,
};

TEST(EnumTableDeathTest, HugeEnum) {
  EXPECT_DEATH(EnumTable<HugeEnum>(
                   {
                       {HugeEnum::k0, "k0"},   {HugeEnum::k1, "k1"},
                       {HugeEnum::k2, "k2"},   {HugeEnum::k3, "k3"},
                       {HugeEnum::k4, "k4"},   {HugeEnum::k5, "k5"},
                       {HugeEnum::k6, "k6"},   {HugeEnum::k7, "k7"},
                       {HugeEnum::k8, "k8"},   {HugeEnum::k9, "k9"},
                       {HugeEnum::k10, "k10"}, {HugeEnum::k11, "k11"},
                       {HugeEnum::k12, "k12"}, {HugeEnum::k13, "k13"},
                       {HugeEnum::k14, "k14"}, {HugeEnum::k15, "k15"},
                       {HugeEnum::k16, "k16"}, {HugeEnum::k17, "k17"},
                       {HugeEnum::k18, "k18"}, {HugeEnum::k19, "k19"},
                       {HugeEnum::k20, "k20"}, {HugeEnum::k21, "k21"},
                       {HugeEnum::k22, "k22"}, {HugeEnum::k23, "k23"},
                       {HugeEnum::k24, "k24"}, {HugeEnum::k25, "k25"},
                       {HugeEnum::k26, "k26"}, {HugeEnum::k27, "k27"},
                       {HugeEnum::k28, "k28"}, {HugeEnum::k29, "k29"},
                       {HugeEnum::k30, "k30"}, {HugeEnum::k31, "k31"},
                       {HugeEnum::k32, "k32"},
                   },
                   HugeEnum::kMaxValue),
               ".*Table too large.");
}

#endif  // NDEBUG

}  // namespace

}  // namespace openscreen
