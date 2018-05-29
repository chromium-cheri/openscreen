// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ip_address.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {

using IPAddressTest = ::testing::Test;

TEST_F(IPAddressTest, ParseV4) {
  IPV4Address address;
  const auto result = IPV4Address::Parse("192.168.0.1", &address);
  ASSERT_TRUE(result);
  const auto expected = std::array<uint8_t, 4>{{192, 168, 0, 1}};
  EXPECT_EQ(expected, address.bytes);
}

TEST_F(IPAddressTest, ParseV4Nondigit) {
  IPV4Address address;
  const auto result = IPV4Address::Parse("192.x3.0.1", &address);
  ASSERT_FALSE(result);
}

TEST_F(IPAddressTest, ParseV4TooFewValues) {
  IPV4Address address;
  const auto result = IPV4Address::Parse("192.3.1", &address);
  ASSERT_FALSE(result);
}

TEST_F(IPAddressTest, ParseV4TooManyValues) {
  IPV4Address address;
  const auto result = IPV4Address::Parse("192.3.2.0.1", &address);
  ASSERT_FALSE(result);
}

TEST_F(IPAddressTest, ParseV4Overflow) {
  IPV4Address address;
  const auto result = IPV4Address::Parse("1920.3.2.1", &address);
  ASSERT_FALSE(result);
}

}  // namespace openscreen
