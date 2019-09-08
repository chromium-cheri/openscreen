// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/network_waiter.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace platform {
namespace {

using namespace ::testing;

class MockSubscriber : public NetworkWaiter::Subscriber {
 public:
  MOCK_METHOD0(GetFds, std::vector<int>());
  MOCK_METHOD1(ProcessReadyFd, void(int));
};

class TestingNetworkWaiter : public NetworkWaiter {
 public:
  MOCK_METHOD2(AwaitSocketsReadable,
               ErrorOr<std::vector<int>>(const std::vector<int>&,
                                         const Clock::duration&));

  using NetworkWaiter::ProcessFds;
};

}  // namespace

TEST(NetworkWaiterTest, BubblesUpAwaitSocketsReadableErrors) {
  MockSubscriber subscriber;
  TestingNetworkWaiter waiter;
  waiter.Subscribe(&subscriber);
  Error::Code response = Error::Code::kAgain;
  EXPECT_CALL(subscriber, GetFds())
      .WillOnce(Return(ByMove(std::vector<int>{0, 1, 2})));
  EXPECT_CALL(subscriber, ProcessReadyFd(_)).Times(0);
  EXPECT_CALL(waiter, AwaitSocketsReadable(_, _))
      .WillOnce(Return(ByMove(response)));
  waiter.ProcessFds(Clock::duration{0});
}

TEST(NetworkWaiterTest, WatchedSocketsReturnedToCorrectSubscribers) {
  MockSubscriber subscriber;
  MockSubscriber subscriber2;
  TestingNetworkWaiter waiter;
  waiter.Subscribe(&subscriber);
  waiter.Subscribe(&subscriber2);
  EXPECT_CALL(subscriber, GetFds())
      .WillOnce(Return(ByMove(std::vector<int>{0, 2})));
  EXPECT_CALL(subscriber2, GetFds())
      .WillOnce(Return(ByMove(std::vector<int>{1, 3})));
  EXPECT_CALL(subscriber, ProcessReadyFd(0)).Times(1);
  EXPECT_CALL(subscriber, ProcessReadyFd(2)).Times(1);
  EXPECT_CALL(subscriber2, ProcessReadyFd(1)).Times(1);
  EXPECT_CALL(subscriber2, ProcessReadyFd(3)).Times(1);
  EXPECT_CALL(waiter, AwaitSocketsReadable(_, _))
      .WillOnce(Return(ByMove(std::vector<int>{0, 1, 2, 3})));
  waiter.ProcessFds(Clock::duration{0});
}

}  // namespace platform
}  // namespace openscreen
