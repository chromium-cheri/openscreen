// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/network_loop.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/api/event_waiter.h"

namespace openscreen {
namespace platform {

using namespace ::testing;
using ::testing::_;
using ::testing::Invoke;

// Mock SocketHandler
class MockSocketHandler final : public SocketHandler {
 public:
  MOCK_METHOD1(Watch, void(const UdpSocket&));
  MOCK_METHOD1(IsChanged, bool(const UdpSocket&));
  MOCK_METHOD0(Clear, void());
};

// Mock event waiter
class MockEventWaiter final : public EventWaiter {
 public:
  MOCK_METHOD3(WaitForEvents,
               Error(Clock::duration, SocketHandler*, SocketHandler*));
};

// Mock UdpSocket class used for UTs.
class MockUdpSocket final : public UdpSocket {
 public:
  MockUdpSocket(){};
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

  void RunUntilStopped() override {}
  void RequestStopSoon() override {}

  uint32_t tasks_posted;
  uint32_t delayed_tasks_posted;
};

// Class extending NetworkWaiter to allow for looking at protected data.
class TestingNetworkWaiter final : public NetworkLoop {
 public:
  TestingNetworkWaiter(EventWaiter* waiter,
                       SocketHandler* reads,
                       SocketHandler* writes)
      : NetworkLoop(std::unique_ptr<EventWaiter>(waiter),
                    std::unique_ptr<SocketHandler>(reads),
                    std::unique_ptr<SocketHandler>(writes)) {}

  bool IsMappedRead(UdpSocket* socket) {
    auto result = read_callbacks_.find(socket);
    return result != read_callbacks_.end();
  }

  bool IsMappedWrite(UdpSocket* socket) {
    auto result = write_callbacks_.find(socket);
    return result != write_callbacks_.end();
  }

  // Public method to call wait, since usually this method is internally
  // callable only.
  Error WaitTesting(Clock::duration timeout) { return Wait(timeout); }

  MOCK_METHOD1(ReadData,
               std::unique_ptr<UdpReadCallback::Packet>(UdpSocket* socket));
};

class MockCallbacks {
 public:
  std::function<void(std::unique_ptr<UdpReadCallback::Packet>)>
  GetReadCallback() {
    return std::bind(&MockCallbacks::ReadCallback, this, std::placeholders::_1);
  }

  std::function<void()> GetWriteCallback() {
    return std::bind(&MockCallbacks::WriteCallback, this);
  }

  void ReadCallback(std::unique_ptr<UdpReadCallback::Packet> packet) {
    ReadCallbackInternal();
  }

  MOCK_METHOD0(ReadCallbackInternal, void());
  MOCK_METHOD0(WriteCallback, void());
};

TEST(NetworkLoopTest, WatchReadableSucceeds) {
  MockEventWaiter* mock_waiter = new MockEventWaiter();
  MockSocketHandler* read_handler = new MockSocketHandler();
  MockSocketHandler* write_handler = new MockSocketHandler();
  MockUdpSocket socket;
  TestingNetworkWaiter network_waiter(mock_waiter, read_handler, write_handler);
  MockCallbacks callbacks;

  EXPECT_EQ(network_waiter.IsMappedRead(&socket), false);

  auto callback = callbacks.GetReadCallback();
  EXPECT_EQ(network_waiter.ReadRepeatedly(&socket, callback).code(),
            Error::Code::kNone);
  ;
  EXPECT_EQ(network_waiter.IsMappedRead(&socket), true);

  auto callback2 = callbacks.GetReadCallback();
  EXPECT_EQ(network_waiter.ReadRepeatedly(&socket, callback2).code(),
            Error::Code::kIOFailure);
  ;
  EXPECT_EQ(network_waiter.IsMappedRead(&socket), true);
}

TEST(NetworkLoopTest, UnwatchReadableSucceeds) {
  MockEventWaiter* mock_waiter = new MockEventWaiter();
  MockSocketHandler* read_handler = new MockSocketHandler();
  MockSocketHandler* write_handler = new MockSocketHandler();
  MockUdpSocket socket;
  TestingNetworkWaiter network_waiter(mock_waiter, read_handler, write_handler);
  MockCallbacks callbacks;

  auto callback = callbacks.GetReadCallback();
  EXPECT_EQ(network_waiter.CancelRead(&socket).code(),
            Error::Code::kNoItemFound);
  EXPECT_EQ(network_waiter.IsMappedRead(&socket), false);

  EXPECT_EQ(network_waiter.ReadRepeatedly(&socket, callback).code(),
            Error::Code::kNone);

  EXPECT_EQ(network_waiter.CancelRead(&socket).code(), Error::Code::kNone);
  EXPECT_EQ(network_waiter.IsMappedRead(&socket), false);

  EXPECT_EQ(network_waiter.CancelRead(&socket).code(),
            Error::Code::kNoItemFound);
}

TEST(NetworkLoopTest, WatchWritableSucceeds) {
  MockEventWaiter* mock_waiter = new MockEventWaiter();
  MockSocketHandler* read_handler = new MockSocketHandler();
  MockSocketHandler* write_handler = new MockSocketHandler();
  MockUdpSocket socket;
  TestingNetworkWaiter network_waiter(mock_waiter, read_handler, write_handler);
  MockCallbacks callbacks;

  EXPECT_EQ(network_waiter.IsMappedWrite(&socket), false);

  std::function<void()> callback = callbacks.GetWriteCallback();
  EXPECT_EQ(network_waiter.WriteAll(&socket, callback).code(),
            Error::Code::kNone);
  EXPECT_EQ(network_waiter.IsMappedWrite(&socket), true);

  std::function<void()> callback2 = callbacks.GetWriteCallback();
  EXPECT_EQ(network_waiter.WriteAll(&socket, callback2).code(),
            Error::Code::kIOFailure);
  EXPECT_EQ(network_waiter.IsMappedWrite(&socket), true);
}

TEST(NetworkLoopTest, UnwatchWritableSucceeds) {
  MockEventWaiter* mock_waiter = new MockEventWaiter();
  MockSocketHandler* read_handler = new MockSocketHandler();
  MockSocketHandler* write_handler = new MockSocketHandler();
  MockUdpSocket socket;
  TestingNetworkWaiter network_waiter(mock_waiter, read_handler, write_handler);
  MockCallbacks callbacks;

  std::function<void()> callback = callbacks.GetWriteCallback();
  EXPECT_EQ(network_waiter.CancelWriteAll(&socket).code(),
            Error::Code::kNoItemFound);
  EXPECT_EQ(network_waiter.IsMappedWrite(&socket), false);

  EXPECT_EQ(network_waiter.WriteAll(&socket, callback).code(),
            Error::Code::kNone);

  EXPECT_EQ(network_waiter.CancelWriteAll(&socket).code(), Error::Code::kNone);
  EXPECT_EQ(network_waiter.IsMappedWrite(&socket), false);

  EXPECT_EQ(network_waiter.CancelWriteAll(&socket).code(),
            Error::Code::kNoItemFound);
}

TEST(NetworkLoopTest, WaitErrorOnNoTaskRunner) {
  MockEventWaiter* mock_waiter = new MockEventWaiter();
  MockSocketHandler* read_handler = new MockSocketHandler();
  MockSocketHandler* write_handler = new MockSocketHandler();
  MockUdpSocket socket;
  TestingNetworkWaiter network_waiter(mock_waiter, read_handler, write_handler);
  auto timeout = Clock::duration(0);

  EXPECT_EQ(network_waiter.WaitTesting(timeout),
            Error::Code::kInitializationFailure);
}

TEST(NetworkLoopTest, WaitBubblesUpWaitForEventsErrors) {
  MockEventWaiter* mock_waiter = new MockEventWaiter();
  MockSocketHandler* read_handler = new MockSocketHandler();
  MockSocketHandler* write_handler = new MockSocketHandler();
  MockUdpSocket socket;
  TestingNetworkWaiter network_waiter(mock_waiter, read_handler, write_handler);
  MockTaskRunner task_runner;
  network_waiter.SetTaskRunner(&task_runner);
  auto timeout = Clock::duration(0);

  Error result = {Error::Code::kAgain};
  EXPECT_CALL(*mock_waiter, WaitForEvents(timeout, read_handler, write_handler))
      .WillOnce(Return(result));
  EXPECT_CALL(*read_handler, IsChanged(_)).Times(0);
  EXPECT_CALL(*write_handler, IsChanged(_)).Times(0);
  EXPECT_EQ(network_waiter.WaitTesting(timeout), Error::Code::kAgain);

  result = {Error::Code::kNoItemFound};
  EXPECT_CALL(*mock_waiter, WaitForEvents(timeout, read_handler, write_handler))
      .WillOnce(Return(result));
  EXPECT_CALL(*read_handler, IsChanged(_)).Times(0);
  EXPECT_CALL(*write_handler, IsChanged(_)).Times(0);
  EXPECT_EQ(network_waiter.WaitTesting(timeout), Error::Code::kNoItemFound);
}

TEST(NetworkLoopTest, WaitReturnsSuccessfulOnNoEvents) {
  MockEventWaiter* mock_waiter = new MockEventWaiter();
  MockSocketHandler* read_handler = new MockSocketHandler();
  MockSocketHandler* write_handler = new MockSocketHandler();
  MockUdpSocket socket;
  TestingNetworkWaiter network_waiter(mock_waiter, read_handler, write_handler);
  MockTaskRunner task_runner;
  network_waiter.SetTaskRunner(&task_runner);
  auto timeout = Clock::duration(0);

  EXPECT_CALL(*read_handler, IsChanged(_)).Times(0);
  EXPECT_CALL(*write_handler, IsChanged(_)).Times(0);
  EXPECT_CALL(*mock_waiter, WaitForEvents(timeout, read_handler, write_handler))
      .WillOnce(Return(Error::Code::kNone));
  EXPECT_EQ(network_waiter.WaitTesting(timeout), Error::Code::kNone);
}

TEST(NetworkLoopTest, WaitSuccessfulReadAndCallCallback) {
  MockEventWaiter* mock_waiter = new MockEventWaiter();
  MockSocketHandler* read_handler = new MockSocketHandler();
  MockSocketHandler* write_handler = new MockSocketHandler();
  MockUdpSocket socket;
  TestingNetworkWaiter network_waiter(mock_waiter, read_handler, write_handler);
  MockTaskRunner task_runner;
  network_waiter.SetTaskRunner(&task_runner);
  auto timeout = Clock::duration(0);
  auto packet = std::make_unique<UdpReadCallback::Packet>();
  MockCallbacks callbacks;

  network_waiter.ReadRepeatedly(&socket, callbacks.GetReadCallback());

  EXPECT_CALL(*mock_waiter, WaitForEvents(timeout, read_handler, write_handler))
      .WillOnce(Return(Error::Code::kNone));
  EXPECT_CALL(callbacks, ReadCallbackInternal()).Times(1);
  EXPECT_CALL(*read_handler, IsChanged(_)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*write_handler, IsChanged(_)).Times(0);
  EXPECT_CALL(network_waiter, ReadData(&socket))
      .WillOnce(Return(ByMove(std::move(packet))));
  EXPECT_EQ(network_waiter.WaitTesting(timeout), Error::Code::kNone);
  EXPECT_EQ(task_runner.tasks_posted, uint32_t{1});
}

TEST(NetworkLoopTest, WaitSuccessfulWriteAndCallCallback) {
  MockEventWaiter* mock_waiter = new MockEventWaiter();
  MockSocketHandler* read_handler = new MockSocketHandler();
  MockSocketHandler* write_handler = new MockSocketHandler();
  MockUdpSocket socket;
  TestingNetworkWaiter network_waiter(mock_waiter, read_handler, write_handler);
  MockTaskRunner task_runner;
  network_waiter.SetTaskRunner(&task_runner);
  auto timeout = Clock::duration(0);
  MockCallbacks callbacks;

  network_waiter.WriteAll(&socket, callbacks.GetWriteCallback());

  EXPECT_CALL(*mock_waiter, WaitForEvents(timeout, read_handler, write_handler))
      .WillOnce(Return(Error::Code::kNone));
  EXPECT_CALL(callbacks, WriteCallback()).Times(1);
  EXPECT_CALL(*read_handler, IsChanged(_)).Times(0);
  EXPECT_CALL(*write_handler, IsChanged(_)).Times(1).WillOnce(Return(true));
  EXPECT_EQ(network_waiter.WaitTesting(timeout), Error::Code::kNone);
  EXPECT_EQ(task_runner.tasks_posted, uint32_t{1});
}

}  // namespace platform
}  // namespace openscreen
