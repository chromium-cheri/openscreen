// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/bytes_view.h"

#include "gtest/gtest.h"

namespace {

constexpr char kSampleBytes[] = "googleplex";
const uint8_t* kSampleData =
    reinterpret_cast<const uint8_t* const>(&kSampleBytes[0]);
constexpr size_t kSampleSize =
    sizeof(kSampleBytes) - 1;  // Ignore null terminator.

}  // namespace

namespace openscreen {

TEST(BytesViewTest, TestBasics) {
  bytes_view nullView;
  EXPECT_EQ(nullView.data(), nullptr);
  EXPECT_EQ(nullView.size(), size_t{0});
  EXPECT_TRUE(nullView.empty());

  bytes_view googlePlex = bytes_view(kSampleData, kSampleSize);
  EXPECT_EQ(googlePlex.data(), kSampleData);
  EXPECT_EQ(googlePlex.size(), kSampleSize);
  EXPECT_FALSE(googlePlex.empty());

  EXPECT_EQ(googlePlex[0], 'g');
  EXPECT_EQ(googlePlex[9], 'x');

  bytes_view copyBytes = googlePlex;
  EXPECT_EQ(copyBytes.data(), googlePlex.data());
  EXPECT_EQ(copyBytes.size(), googlePlex.size());

  bytes_view firstBytes(googlePlex.first(4));
  EXPECT_EQ(firstBytes.data(), googlePlex.data());
  EXPECT_EQ(firstBytes.size(), size_t{4});
  EXPECT_EQ(firstBytes[0], 'g');
  EXPECT_EQ(firstBytes[3], 'g');

  bytes_view lastBytes(googlePlex.last(4));
  EXPECT_EQ(lastBytes.data(), googlePlex.data() + 6);
  EXPECT_EQ(lastBytes.size(), size_t{4});
  EXPECT_EQ(lastBytes[0], 'p');
  EXPECT_EQ(lastBytes[3], 'x');

  bytes_view middleBytes(googlePlex.subspan(2, 4));
  EXPECT_EQ(middleBytes.data(), googlePlex.data() + 2);
  EXPECT_EQ(middleBytes.size(), size_t{4});
  EXPECT_EQ(middleBytes[0], 'o');
  EXPECT_EQ(middleBytes[3], 'e');
}

}  // namespace openscreen
