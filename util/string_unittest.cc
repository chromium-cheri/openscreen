// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/string.h"

#include <array>
#include <cstdio>
#include <iomanip>
#include <sstream>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::ElementsAre;
namespace openscreen {

TEST(StringTest, HexToBytesValidInputSucceeds) {
  std::array<uint8_t, 1> out_1;
  EXPECT_EQ(Error::None(), HexToBytes("00", out_1.data(), out_1.size()));
  EXPECT_THAT(out_1, ElementsAre(0x00));
  EXPECT_EQ(Error::None(), HexToBytes("4B", out_1.data(), out_1.size()));
  EXPECT_THAT(out_1, ElementsAre(0x4B));
  EXPECT_EQ(Error::None(), HexToBytes("FF", out_1.data(), out_1.size()));
  EXPECT_THAT(out_1, ElementsAre(0xFF));

  std::array<uint8_t, 2> out_2;
  EXPECT_EQ(Error::None(), HexToBytes("5A2B", out_2.data(), out_2.size()));
  EXPECT_THAT(out_2, ElementsAre(0x5a, 0x2b));
  EXPECT_EQ(Error::None(), HexToBytes("FFFF", out_2.data(), out_2.size()));
  EXPECT_THAT(out_2, ElementsAre(0xff, 0xff));

  std::array<uint8_t, 5> out_5;
  EXPECT_EQ(Error::None(),
            HexToBytes("5A2B4C1D0E", out_5.data(), out_5.size()));
  EXPECT_THAT(out_5, ElementsAre(0x5a, 0x2b, 0x4c, 0x1d, 0x0e));
}

TEST(StringTest, HexKeyToBytesIsCaseInsensitive) {
  std::array<uint8_t, 2> out;
  EXPECT_EQ(Error::None(), HexToBytes("5a2b", out.data(), out.size()));
  EXPECT_THAT(out, ElementsAre(0x5a, 0x2b));
  EXPECT_EQ(Error::None(), HexToBytes("5A2B", out.data(), out.size()));
  EXPECT_THAT(out, ElementsAre(0x5a, 0x2b));
}

TEST(StringTest, HexKeyToBytesInvalidInputFails) {
  std::array<uint8_t, 2> out;
  EXPECT_NE(Error::None(), HexToBytes("", out.data(), out.size()));
  EXPECT_NE(Error::None(), HexToBytes("0b1111111111", out.data(), out.size()));
}

TEST(StringTest, HexKeyToBytesWorksWithPrefix) {
  std::array<uint8_t, 1> out;
  EXPECT_EQ(Error::None(), HexToBytes("0x01", out.data(), out.size()));
  EXPECT_THAT(out, ElementsAre(0x01));
  EXPECT_EQ(Error::None(), HexToBytes("0xFF", out.data(), out.size()));
  EXPECT_THAT(out, ElementsAre(0xff));
}

TEST(StringTest, HexKeyToBytesWorksIfBufferOverlyLarge) {
  std::array<uint8_t, 5> out;

  EXPECT_EQ(Error::None(), HexToBytes("0x01", out.data(), out.size()));
  EXPECT_THAT(out, ElementsAre(0, 0, 0, 0, 0x01));

  EXPECT_EQ(Error::None(), HexToBytes("0", out.data(), out.size()));
  EXPECT_THAT(out, ElementsAre(0, 0, 0, 0, 0));
}
}  // namespace openscreen
