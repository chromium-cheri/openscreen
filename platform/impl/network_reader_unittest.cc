// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/network_reader.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/test/fake_network_runner.h"
#include "platform/test/mock_udp_socket.h"

namespace openscreen {
namespace platform {

using namespace ::testing;
using ::testing::_;
using ::testing::Invoke;

// Mock event waiter
class MockNetworkWaiter final : public NetworkWaiter {
 public:
  MOCK_METHOD2(AwaitSocketsReadable,
               ErrorOr<std::vector<UdpSocket*>>(const std::vector<UdpSocket*>&,
                                                const Clock::duration&));
};

// Class extending NetworkWaiter to allow for looking at protected data.
class TestingNetworkWaiter final : public NetworkReader {
 public:
  TestingNetworkWaiter(std::unique_ptr<NetworkWaiter> waiter)
      : NetworkReader(std::move(waiter)) {}

  bool IsMappedRead(UdpSocket* socket) {
    return sockets_.find(socket) != sockets_.end();
  }

  // Public method to call wait, since usually this method is internally
  // callable only.
  Error WaitTesting(Clock::duration timeout) { return WaitAndRead(timeout); }
};

TEST(NetworkReaderTest, WatchReadableSucceeds) {
  std::unique_ptr<NetworkWaiter> mock_waiter =
      std::unique_ptr<NetworkWaiter>(new MockNetworkWaiter());
  auto network_runner = std::make_unique<FakeNetworkRunner>();
  std::unique_ptr<MockUdpSocket> socket =
      std::make_unique<MockUdpSocket>(network_runner.get());
  TestingNetworkWaiter network_waiter(std::move(mock_waiter));

  EXPECT_EQ(network_waiter.IsMappedRead(socket.get()), false);

  EXPECT_EQ(network_waiter.WatchSocket(socket.get()).code(),
            Error::Code::kNone);

  EXPECT_EQ(network_waiter.IsMappedRead(socket.get()), true);

  EXPECT_EQ(network_waiter.WatchSocket(socket.get()).code(),
            Error::Code::kAlreadyListening);

  EXPECT_EQ(network_waiter.IsMappedRead(socket.get()), true);
}

TEST(NetworkReaderTest, UnwatchReadableSucceeds) {
  std::unique_ptr<NetworkWaiter> mock_waiter =
      std::unique_ptr<NetworkWaiter>(new MockNetworkWaiter());
  auto network_runner = std::make_unique<FakeNetworkRunner>();
  std::unique_ptr<MockUdpSocket> socket =
      std::make_unique<MockUdpSocket>(network_runner.get());
  TestingNetworkWaiter network_waiter(std::move(mock_waiter));

  EXPECT_EQ(network_waiter.UnwatchSocket(socket.get(), false),
            Error::Code::kNotRunning);
  EXPECT_FALSE(network_waiter.IsMappedRead(socket.get()));

  EXPECT_EQ(network_waiter.WatchSocket(socket.get()).code(),
            Error::Code::kNone);

  EXPECT_EQ(network_waiter.UnwatchSocket(socket.get(), false),
            Error::Code::kNone);
  EXPECT_FALSE(network_waiter.IsMappedRead(socket.get()));

  EXPECT_EQ(network_waiter.UnwatchSocket(socket.get(), false),
            Error::Code::kNotRunning);
  EXPECT_FALSE(network_waiter.IsMappedRead(socket.get()));
}

TEST(NetworkReaderTest, WaitBubblesUpWaitForEventsErrors) {
  auto* mock_waiter_ptr = new MockNetworkWaiter();
  std::unique_ptr<NetworkWaiter> mock_waiter =
      std::unique_ptr<NetworkWaiter>(mock_waiter_ptr);
  auto network_runner = std::make_unique<FakeNetworkRunner>();
  TestingNetworkWaiter network_waiter(std::move(mock_waiter));
  auto timeout = Clock::duration(0);

  Error::Code response_code = Error::Code::kAgain;
  EXPECT_CALL(*mock_waiter_ptr, AwaitSocketsReadable(_, timeout))
      .WillOnce(Return(ByMove(std::move(response_code))));
  auto result = network_waiter.WaitTesting(timeout);
  EXPECT_EQ(result.code(), response_code);

  response_code = Error::Code::kAlreadyListening;
  EXPECT_CALL(*mock_waiter_ptr, AwaitSocketsReadable(_, timeout))
      .WillOnce(Return(ByMove(std::move(response_code))));
  result = network_waiter.WaitTesting(timeout);
  EXPECT_EQ(result.code(), response_code);
}

TEST(NetworkReaderTest, WaitReturnsSuccessfulOnNoEvents) {
  auto* mock_waiter_ptr = new MockNetworkWaiter();
  std::unique_ptr<NetworkWaiter> mock_waiter =
      std::unique_ptr<NetworkWaiter>(mock_waiter_ptr);
  auto network_runner = std::make_unique<FakeNetworkRunner>();
  TestingNetworkWaiter network_waiter(std::move(mock_waiter));
  auto timeout = Clock::duration(0);

  EXPECT_CALL(*mock_waiter_ptr, AwaitSocketsReadable(_, timeout))
      .WillOnce(Return(ByMove(std::vector<UdpSocket*>{})));
  EXPECT_EQ(network_waiter.WaitTesting(timeout), Error::Code::kNone);
}

TEST(NetworkReaderTest, WaitSuccessfullyCalledOnAllWatchedSockets) {
  auto* mock_waiter_ptr = new MockNetworkWaiter();
  std::unique_ptr<NetworkWaiter> mock_waiter =
      std::unique_ptr<NetworkWaiter>(mock_waiter_ptr);
  auto network_runner = std::make_unique<FakeNetworkRunner>();
  std::unique_ptr<MockUdpSocket> socket =
      std::make_unique<MockUdpSocket>(network_runner.get());
  TestingNetworkWaiter network_waiter(std::move(mock_waiter));
  auto timeout = Clock::duration(0);
  UdpPacket packet;

  network_waiter.WatchSocket(socket.get());
  EXPECT_CALL(
      *mock_waiter_ptr,
      AwaitSocketsReadable(ContainerEq<std::vector<UdpSocket*>>({socket.get()}),
                           timeout))
      .WillOnce(Return(ByMove(std::move(Error::Code::kAgain))));
  EXPECT_EQ(network_waiter.WaitTesting(timeout), Error::Code::kAgain);
}

TEST(NetworkReaderTest, WaitSuccessfulReadAndCallCallback) {
  auto* mock_waiter_ptr = new MockNetworkWaiter();
  std::unique_ptr<NetworkWaiter> mock_waiter =
      std::unique_ptr<NetworkWaiter>(mock_waiter_ptr);
  auto network_runner = std::make_unique<FakeNetworkRunner>();
  MockUdpSocket socket(network_runner.get());
  TestingNetworkWaiter network_waiter(std::move(mock_waiter));
  auto timeout = Clock::duration(0);
  UdpPacket packet;

  network_waiter.WatchSocket(&socket);

  EXPECT_CALL(*mock_waiter_ptr, AwaitSocketsReadable(_, timeout))
      .WillOnce(Return(ByMove(std::vector<UdpSocket*>{&socket})));
  EXPECT_CALL(socket, ReceiveMessage())
      .Times(1)
      .WillOnce(Return(Error::Code::kNone));
  EXPECT_EQ(network_waiter.WaitTesting(timeout), Error::Code::kNone);
}

TEST(NetworkReaderTest, WaitFailsIfReadingSocketFails) {
  auto* mock_waiter_ptr = new MockNetworkWaiter();
  std::unique_ptr<NetworkWaiter> mock_waiter =
      std::unique_ptr<NetworkWaiter>(mock_waiter_ptr);
  auto network_runner = std::make_unique<FakeNetworkRunner>();
  MockUdpSocket socket(network_runner.get());
  TestingNetworkWaiter network_waiter(std::move(mock_waiter));
  auto timeout = Clock::duration(0);

  network_waiter.WatchSocket(&socket);

  EXPECT_CALL(*mock_waiter_ptr, AwaitSocketsReadable(_, timeout))
      .WillOnce(Return(ByMove(std::vector<UdpSocket*>{&socket})));
  EXPECT_CALL(socket, ReceiveMessage())
      .WillOnce(Return(ByMove(Error::Code::kGenericPlatformError)));
  EXPECT_EQ(network_waiter.WaitTesting(timeout),
            Error::Code::kGenericPlatformError);
}

}  // namespace platform
}  // namespace openscreen
