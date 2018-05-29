// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ip_address.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {

using ::testing::ElementsAreArray;

TEST(IPV4AddressTest, Constructors) {
  IPV4Address address1(std::array<uint8_t, 4>{1, 2, 3, 4});
  EXPECT_THAT(address1.bytes, ElementsAreArray({1, 2, 3, 4}));

  uint8_t x[] = {4, 3, 2, 1};
  IPV4Address address2(x);
  EXPECT_THAT(address2.bytes, ElementsAreArray(x));

  IPV4Address address3(&x[0]);
  EXPECT_THAT(address3.bytes, ElementsAreArray(x));

  IPV4Address address4(6, 5, 7, 9);
  EXPECT_THAT(address4.bytes, ElementsAreArray({6, 5, 7, 9}));

  IPV4Address address5(address4);
  EXPECT_THAT(address5.bytes, ElementsAreArray({6, 5, 7, 9}));

  address5 = address1;
  EXPECT_THAT(address5.bytes, ElementsAreArray({1, 2, 3, 4}));
}

TEST(IPV4AddressTest, Comparison) {
  IPV4Address address1;
  EXPECT_EQ(address1, address1);

  IPV4Address address2({4, 3, 2, 1});
  EXPECT_NE(address1, address2);

  IPV4Address address3({4, 3, 2, 1});
  EXPECT_EQ(address2, address3);

  address2 = address1;
  EXPECT_EQ(address1, address2);
}

TEST(IPV4AddressTest, Parse) {
  IPV4Address address;
  const auto result = IPV4Address::Parse("192.168.0.1", &address);
  ASSERT_TRUE(result);
  EXPECT_THAT(address.bytes, ElementsAreArray({192, 168, 0, 1}));
}

TEST(IPV4AddressTest, ParseEmptyValue) {
  IPV4Address address;
  const auto result = IPV4Address::Parse("192..0.1", &address);
  ASSERT_FALSE(result);
}

TEST(IPV4AddressTest, ParseNondigit) {
  IPV4Address address;
  const auto result = IPV4Address::Parse("192.x3.0.1", &address);
  ASSERT_FALSE(result);
}

TEST(IPV4AddressTest, ParseTooFewValues) {
  IPV4Address address;
  const auto result = IPV4Address::Parse("192.3.1", &address);
  ASSERT_FALSE(result);
}

TEST(IPV4AddressTest, ParseTooManyValues) {
  IPV4Address address;
  const auto result = IPV4Address::Parse("192.3.2.0.1", &address);
  ASSERT_FALSE(result);
}

TEST(IPV4AddressTest, ParseOverflow) {
  IPV4Address address;
  const auto result = IPV4Address::Parse("1920.3.2.1", &address);
  ASSERT_FALSE(result);
}

TEST(IPV6AddressTest, Constructors) {
  IPV6Address address1(std::array<uint8_t, 16>(
      {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}));
  EXPECT_THAT(address1.bytes, ElementsAreArray({1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                                                11, 12, 13, 14, 15, 16}));

  const uint8_t x[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  IPV6Address address2(x);
  EXPECT_THAT(address2.bytes, ElementsAreArray(x));

  IPV6Address address3(&x[0]);
  EXPECT_THAT(address3.bytes, ElementsAreArray(x));

  IPV6Address address4(16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);
  EXPECT_THAT(address4.bytes, ElementsAreArray({16, 15, 14, 13, 12, 11, 10, 9,
                                                8, 7, 6, 5, 4, 3, 2, 1}));

  IPV6Address address5(address4);
  EXPECT_THAT(address5.bytes, ElementsAreArray({16, 15, 14, 13, 12, 11, 10, 9,
                                                8, 7, 6, 5, 4, 3, 2, 1}));
}

TEST(IPV6AddressTest, Comparison) {
  IPV6Address address1;
  EXPECT_EQ(address1, address1);

  IPV6Address address2({16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1});
  EXPECT_NE(address1, address2);

  IPV6Address address3({16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1});
  EXPECT_EQ(address2, address3);

  address2 = address1;
  EXPECT_EQ(address1, address2);
}

TEST(IPV6AddressTest, ParseBasic) {
  IPV6Address address;
  const auto result =
      IPV6Address::Parse("abcd:ef01:2345:6789:9876:5432:10FE:DBCA", &address);
  ASSERT_TRUE(result);
  EXPECT_THAT(
      address.bytes,
      ElementsAreArray({0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0x98,
                        0x76, 0x54, 0x32, 0x10, 0xfe, 0xdb, 0xca}));
}

TEST(IPV6AddressTest, ParseDoubleColon) {
  IPV6Address address1;
  const auto result1 =
      IPV6Address::Parse("abcd:ef01:2345:6789:9876:5432::dbca", &address1);
  ASSERT_TRUE(result1);
  EXPECT_THAT(
      address1.bytes,
      ElementsAreArray({0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0x98,
                        0x76, 0x54, 0x32, 0x00, 0x00, 0xdb, 0xca}));
  IPV6Address address2;
  const auto result2 = IPV6Address::Parse("abcd::10fe:dbca", &address2);
  ASSERT_TRUE(result2);
  EXPECT_THAT(
      address2.bytes,
      ElementsAreArray({0xab, 0xcd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x10, 0xfe, 0xdb, 0xca}));

  IPV6Address address3;
  const auto result3 = IPV6Address::Parse("::10fe:dbca", &address3);
  ASSERT_TRUE(result3);
  EXPECT_THAT(
      address3.bytes,
      ElementsAreArray({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x10, 0xfe, 0xdb, 0xca}));

  IPV6Address address4;
  const auto result4 = IPV6Address::Parse("10fe:dbca::", &address4);
  ASSERT_TRUE(result4);
  EXPECT_THAT(
      address4.bytes,
      ElementsAreArray({0x10, 0xfe, 0xdb, 0xca, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
}

TEST(IPV6AddressTest, SmallValues) {
  IPV6Address address;
  const auto result = IPV6Address::Parse("::2:1", &address);
  ASSERT_TRUE(result);
  EXPECT_THAT(
      address.bytes,
      ElementsAreArray({0x0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01}));
}

TEST(IPV6AddressTest, ParseLeadingColon) {
  IPV6Address address;
  const auto result = IPV6Address::Parse(":abcd::dbca", &address);
  ASSERT_FALSE(result);
}

TEST(IPV6AddressTest, ParseTrailingColon) {
  IPV6Address address;
  const auto result = IPV6Address::Parse("abcd::dbca:", &address);
  ASSERT_FALSE(result);
}

TEST(IPV6AddressTest, ParseValueOverflow) {
  IPV6Address address;
  const auto result = IPV6Address::Parse("abcd1::dbca", &address);
  ASSERT_FALSE(result);
}

TEST(IPV6AddressTest, ParseThreeDigitValue) {
  IPV6Address address;
  const auto result = IPV6Address::Parse("::123", &address);
  ASSERT_TRUE(result);
  EXPECT_THAT(
      address.bytes,
      ElementsAreArray({0x0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x23}));
}

}  // namespace openscreen
