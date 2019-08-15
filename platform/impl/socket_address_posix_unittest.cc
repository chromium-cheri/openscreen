// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/socket_address_posix.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace platform {

TEST(SocketAddressPosixTest, IPv4SocketAddressConvertsSuccessfully) {
  const SocketAddressPosix address(IPEndpoint{{10, 0, 0, 1}, 80});

  // sa_len is not consistent for IPv4 addresses, so we don't check it here.
  EXPECT_EQ(address.address()->sa_family, AF_INET);

  const char kExpected[6] = {0, 'P', '\n', 0, 0, 1};
  for (auto i = 0; i < 6; ++i) {
    EXPECT_EQ(address.address()->sa_data[i], kExpected[i]);
  }
}

TEST(SocketAddressPosixTest, IPv6SocketAddressConvertsSuccessfully) {
  const SocketAddressPosix address(
      IPEndpoint{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}, 80});

  EXPECT_EQ(address.address()->sa_len, 10);
  EXPECT_EQ(address.address()->sa_family, AF_INET6);

  const char kExpected[14] = {0, 'P', 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8};
  EXPECT_THAT(address.address()->sa_data, testing::ElementsAreArray(kExpected));
}
}  // namespace platform
}  // namespace openscreen