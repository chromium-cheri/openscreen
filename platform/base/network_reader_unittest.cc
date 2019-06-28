// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/network_reader.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/api/event_waiter.h"

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

// Mock UdpSocket class used for UTs.
class MockUdpSocket final : public UdpSocket {
 public:
  MockUdpSocket() = default;
  ~MockUdpSocket() = default;
};

// Mock Task Runner
class MockTaskRunner final : public TaskRunner {
 public:
  MockTaskRunner() {
    tasks_posted = 0;
    delayed_tasks_posted = 0;
  }

  void PostPackagedTask(Task t) override {
    tasks_posted++;
    t();
  }

  void PostPackagedTaskWithDelay(Task t, Clock::duration duration) override {
    delayed_tasks_posted++;
    t();
  }

  uint32_t tasks_posted;
  uint32_t delayed_tasks_posted;
};

// Class extending NetworkWaiter to allow for looking at protected data.
class TestingNetworkWaiter final : public NetworkReader {
 public:
  TestingNetworkWaiter(NetworkWaiter* waiter, TaskRunner* task_runner)
      : NetworkReader(task_runner, std::unique_ptr<NetworkWaiter>(waiter)) {}

  bool IsMappedRead(UdpSocket* socket) {
    auto result = read_callbacks_.find(socket);
    return result != read_callbacks_.end();
  }

  // Public method to call wait, since usually this method is internally
  // callable only.
  Error WaitTesting(Clock::duration timeout) { return WaitAndRead(timeout); }

  MOCK_METHOD1(
      ReadFromSocket,
      ErrorOr<std::unique_ptr<UdpReadCallback::Packet>>(UdpSocket* socket));
};

class MockCallbacks {
 public:
  std::function<void(std::unique_ptr<UdpReadCallback::Packet>)>
  GetReadCallback() {
    return [this](std::unique_ptr<UdpReadCallback::Packet> packet) {
      this->ReadCallback(std::move(packet));
    };
  }

  std::function<void()> GetWriteCallback() {
    return [this]() { this->WriteCallback(); };
  }

  void ReadCallback(std::unique_ptr<UdpReadCallback::Packet> packet) {
    ReadCallbackInternal();
  }

  MOCK_METHOD0(ReadCallbackInternal, void());
  MOCK_METHOD0(WriteCallback, void());
};

TEST(NetworkReaderTest, WatchReadableSucceeds) {
  MockNetworkWaiter* mock_waiter = new MockNetworkWaiter();
  MockUdpSocket* socket = new MockUdpSocket();
  MockTaskRunner* task_runner = new MockTaskRunner();
  TestingNetworkWaiter network_waiter(mock_waiter, task_runner);
  MockCallbacks callbacks;

  EXPECT_EQ(network_waiter.IsMappedRead(socket), false);

  auto callback = callbacks.GetReadCallback();
  EXPECT_EQ(network_waiter.ReadRepeatedly(socket, callback).code(),
            Error::Code::kNone);

  EXPECT_EQ(network_waiter.IsMappedRead(socket), true);

  auto callback2 = callbacks.GetReadCallback();
  EXPECT_EQ(network_waiter.ReadRepeatedly(socket, callback2).code(),
            Error::Code::kIOFailure);

  EXPECT_EQ(network_waiter.IsMappedRead(socket), true);
}

TEST(NetworkReaderTest, UnwatchReadableSucceeds) {
  MockNetworkWaiter* mock_waiter = new MockNetworkWaiter();
  MockUdpSocket* socket = new MockUdpSocket();
  MockTaskRunner* task_runner = new MockTaskRunner();
  TestingNetworkWaiter network_waiter(mock_waiter, task_runner);
  MockCallbacks callbacks;

  auto callback = callbacks.GetReadCallback();
  EXPECT_FALSE(network_waiter.CancelRead(socket));
  EXPECT_FALSE(network_waiter.IsMappedRead(socket));

  EXPECT_EQ(network_waiter.ReadRepeatedly(socket, callback).code(),
            Error::Code::kNone);

  EXPECT_TRUE(network_waiter.CancelRead(socket));
  EXPECT_FALSE(network_waiter.IsMappedRead(socket));

  EXPECT_FALSE(network_waiter.CancelRead(socket));
}

TEST(NetworkReaderTest, WaitBubblesUpWaitForEventsErrors) {
  MockNetworkWaiter* mock_waiter = new MockNetworkWaiter();
  MockTaskRunner* task_runner = new MockTaskRunner();
  TestingNetworkWaiter network_waiter(mock_waiter, task_runner);
  auto timeout = Clock::duration(0);

  Error::Code response_code = Error::Code::kAgain;
  EXPECT_CALL(*mock_waiter, AwaitSocketsReadable(_, timeout))
      .WillOnce(Return(ByMove(std::move(response_code))));
  auto result = network_waiter.WaitTesting(timeout);
  EXPECT_EQ(result.code(), response_code);

  response_code = Error::Code::kAlreadyListening;
  EXPECT_CALL(*mock_waiter, AwaitSocketsReadable(_, timeout))
      .WillOnce(Return(ByMove(std::move(response_code))));
  result = network_waiter.WaitTesting(timeout);
  EXPECT_EQ(result.code(), response_code);
}

TEST(NetworkReaderTest, WaitReturnsSuccessfulOnNoEvents) {
  MockNetworkWaiter* mock_waiter = new MockNetworkWaiter();
  MockTaskRunner* task_runner = new MockTaskRunner();
  TestingNetworkWaiter network_waiter(mock_waiter, task_runner);
  auto timeout = Clock::duration(0);

  EXPECT_CALL(*mock_waiter, AwaitSocketsReadable(_, timeout))
      .WillOnce(Return(ByMove(std::vector<UdpSocket*>{})));
  EXPECT_EQ(network_waiter.WaitTesting(timeout), Error::Code::kNone);
}

TEST(NetworkReaderTest, WaitSuccessfullyCalledOnAllWatchedSockets) {
  MockNetworkWaiter* mock_waiter = new MockNetworkWaiter();
  MockUdpSocket* socket = new MockUdpSocket();
  MockTaskRunner* task_runner = new MockTaskRunner();
  TestingNetworkWaiter network_waiter(mock_waiter, task_runner);
  auto timeout = Clock::duration(0);
  auto packet = std::make_unique<UdpReadCallback::Packet>();
  MockCallbacks callbacks;

  network_waiter.ReadRepeatedly(socket, callbacks.GetReadCallback());
  EXPECT_CALL(*mock_waiter,
              AwaitSocketsReadable(
                  ContainerEq<std::vector<UdpSocket*>>({socket}), timeout))
      .WillOnce(Return(ByMove(std::move(Error::Code::kAgain))));
  EXPECT_EQ(network_waiter.WaitTesting(timeout), Error::Code::kAgain);
}

TEST(NetworkReaderTest, WaitSuccessfulReadAndCallCallback) {
  MockNetworkWaiter* mock_waiter = new MockNetworkWaiter();
  MockUdpSocket* socket = new MockUdpSocket();
  MockTaskRunner* task_runner = new MockTaskRunner();
  TestingNetworkWaiter network_waiter(mock_waiter, task_runner);
  auto timeout = Clock::duration(0);
  auto packet = std::make_unique<UdpReadCallback::Packet>();
  MockCallbacks callbacks;

  network_waiter.ReadRepeatedly(socket, callbacks.GetReadCallback());

  EXPECT_CALL(*mock_waiter, AwaitSocketsReadable(_, timeout))
      .WillOnce(Return(ByMove(std::vector<UdpSocket*>{socket})));
  EXPECT_CALL(callbacks, ReadCallbackInternal()).Times(1);
  EXPECT_CALL(network_waiter, ReadFromSocket(socket))
      .WillOnce(Return(ByMove(std::move(packet))));
  EXPECT_EQ(network_waiter.WaitTesting(timeout), Error::Code::kNone);
  EXPECT_EQ(task_runner->tasks_posted, uint32_t{1});
}

TEST(NetworkReaderTest, WaitFailsIfReadingSocketFails) {
  MockNetworkWaiter* mock_waiter = new MockNetworkWaiter();
  MockUdpSocket* socket = new MockUdpSocket();
  MockTaskRunner* task_runner = new MockTaskRunner();
  TestingNetworkWaiter network_waiter(mock_waiter, task_runner);
  auto timeout = Clock::duration(0);
  auto packet = std::make_unique<UdpReadCallback::Packet>();
  MockCallbacks callbacks;

  network_waiter.ReadRepeatedly(socket, callbacks.GetReadCallback());

  EXPECT_CALL(*mock_waiter, AwaitSocketsReadable(_, timeout))
      .WillOnce(Return(ByMove(std::vector<UdpSocket*>{socket})));
  EXPECT_CALL(callbacks, ReadCallbackInternal()).Times(0);
  EXPECT_CALL(network_waiter, ReadFromSocket(socket))
      .WillOnce(Return(ByMove(Error::Code::kGenericPlatformError)));
  EXPECT_EQ(network_waiter.WaitTesting(timeout),
            Error::Code::kGenericPlatformError);
}

}  // namespace platform
}  // namespace openscreen
