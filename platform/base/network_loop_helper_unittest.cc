// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/network_loop_helper.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace NetworkLoop {

using testing::_;

class MockNetworkOperation : public NetworkOperation {
 public:
  MOCK_METHOD1(PerformNetworkingOperations, void(Clock::duration timeout));
};

TEST(NetworkLoopHelperTest, TestOperationsCorrectlyWrapped) {
  auto first = std::make_unique<MockNetworkOperation>();
  auto second = std::make_unique<MockNetworkOperation>();
  constexpr Clock::duration timeout = Clock::duration{0};

  EXPECT_CALL(*first, PerformNetworkingOperations(_)).Times(0);
  EXPECT_CALL(*second, PerformNetworkingOperations(_)).Times(0);

  std::vector<std::function<void(Clock::duration)>> result =
      GetOperations(first, second);

  EXPECT_CALL(*first, PerformNetworkingOperations(_)).Times(1);
  EXPECT_CALL(*second, PerformNetworkingOperations(_)).Times(1);
  EXPECT_EQ(result.size(), size_t{2});
  for (auto function : result) {
    function(timeout);
  }
}

}  // namespace NetworkLoop
}  // namespace openscreen
