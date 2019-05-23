// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/network_loop.h"

#include "platform/api/event_waiter.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {
namespace platform {

using namespace ::testing;
using ::testing::_;
using ::testing::Invoke;

// Mock event waiter
class MockEventWaiter : public EventWaiter {
 public:
  MOCK_METHOD1(WatchUdpSocketReadable, Error(UdpSocket*));
  MOCK_METHOD1(StopWatchingUdpSocketReadable, Error(UdpSocket*));
  MOCK_METHOD1(WatchUdpSocketWritable, Error(UdpSocket*));
  MOCK_METHOD1(StopWatchingUdpSocketWritable, Error(UdpSocket*));
  MOCK_METHOD0(WatchNetworkChange, Error());
  MOCK_METHOD0(StopWatchingNetworkChange, Error());
  MOCK_METHOD1(WaitForEvents, ErrorOr<Events>(Clock::duration));
  MOCK_METHOD0(GetWakeUpHandler, WakeUpHandler*());
};

// Mock WakeUpHandler
class MockWakeUpHandler : public WakeUpHandler {
 public:
  MOCK_METHOD0(Set, void());
  MOCK_METHOD0(Clear, void());
};

// Mock Callbacks for testing.
class MockReadCallback final : public UdpReadCallback {
 public:
  MOCK_METHOD2(OnRead, void(Packet*, NetworkLoop*));
};

class MockWriteCallback final : public UdpWriteCallback {
 public:
  MOCK_METHOD1(OnWrite, void(NetworkLoop*));
};

// Mock UdpSocket class used for UTs.
class MockUdpSocket : public UdpSocket {
 public:
  MockUdpSocket(){};
};

// Mock Task Runner
class MockTaskRunner : public TaskRunner {
 public:
  uint32_t tasks_posted;

  MockTaskRunner() { tasks_posted = 0; }

  void PostTask(Task t) {
    tasks_posted = 1;
    t();
  }

  MOCK_METHOD2(PostTaskWithDelay, void(Task, Clock::duration));
};

// Class extending NetworkWaiter to allow for looking at protected data.
class TestingNetworkWaiter : public NetworkLoop {
 public:
  TestingNetworkWaiter(EventWaiter* waiter,
                       std::function<WakeUpHandler*()> handler_factory)
      : NetworkLoop(waiter, handler_factory) {}

  UdpReadCallback* IsMappedRead(UdpSocket* socket) {
    auto result = read_callbacks_.find(socket);
    return result != read_callbacks_.end() ? result->second : nullptr;
  }

  UdpWriteCallback* IsMappedWrite(UdpSocket* socket) {
    auto result = write_callbacks_.find(socket);
    return result != write_callbacks_.end() ? result->second : nullptr;
  }

  MOCK_METHOD1(ReadData, UdpReadCallback::Packet*(UdpSocket* socket));
};

TEST(NetworkWaiterTest, WatchReadableSucceeds) {
  MockEventWaiter mock_waiter;
  MockUdpSocket socket;
  std::function<WakeUpHandler*()> wake_up_factory = []() {
    return new MockWakeUpHandler();
  };
  TestingNetworkWaiter network_waiter(&mock_waiter, wake_up_factory);
  MockReadCallback callback;

  EXPECT_CALL(mock_waiter, WatchUdpSocketReadable(&socket)).Times(1);
  network_waiter.ReadAll(&socket, &callback);
  EXPECT_EQ(network_waiter.IsMappedRead(&socket), &callback);

  MockReadCallback callback2;
  EXPECT_CALL(mock_waiter, WatchUdpSocketReadable(&socket)).Times(1);
  network_waiter.ReadAll(&socket, &callback2);
  EXPECT_EQ(network_waiter.IsMappedRead(&socket), &callback2);
}

TEST(NetworkWaiterTest, UnwatchReadableSucceeds) {
  MockEventWaiter mock_waiter;
  MockUdpSocket socket;
  std::function<WakeUpHandler*()> wake_up_factory = []() {
    return new MockWakeUpHandler();
  };
  TestingNetworkWaiter network_waiter(&mock_waiter, wake_up_factory);
  MockReadCallback callback;

  EXPECT_CALL(mock_waiter, StopWatchingUdpSocketReadable(&socket)).Times(1);
  network_waiter.CancelReadAll(&socket);
  EXPECT_EQ(network_waiter.IsMappedRead(&socket), nullptr);

  EXPECT_CALL(mock_waiter, WatchUdpSocketReadable(&socket)).Times(1);
  network_waiter.ReadAll(&socket, &callback);

  EXPECT_CALL(mock_waiter, StopWatchingUdpSocketReadable(&socket)).Times(1);
  network_waiter.CancelReadAll(&socket);
  EXPECT_EQ(network_waiter.IsMappedRead(&socket), nullptr);
}

TEST(NetworkWaiterTest, WatchWritableSucceeds) {
  MockEventWaiter mock_waiter;
  MockUdpSocket socket;
  std::function<WakeUpHandler*()> wake_up_factory = []() {
    return new MockWakeUpHandler();
  };
  TestingNetworkWaiter network_waiter(&mock_waiter, wake_up_factory);
  MockWriteCallback callback;

  EXPECT_CALL(mock_waiter, WatchUdpSocketWritable(&socket)).Times(1);
  network_waiter.WriteAll(&socket, &callback);
  EXPECT_EQ(network_waiter.IsMappedWrite(&socket), &callback);

  MockWriteCallback callback2;
  EXPECT_CALL(mock_waiter, WatchUdpSocketWritable(&socket)).Times(1);
  network_waiter.WriteAll(&socket, &callback2);
  EXPECT_EQ(network_waiter.IsMappedWrite(&socket), &callback2);
}

TEST(NetworkWaiterTest, UnwatchWritableSucceeds) {
  MockEventWaiter mock_waiter;
  MockUdpSocket socket;
  std::function<WakeUpHandler*()> wake_up_factory = []() {
    return new MockWakeUpHandler();
  };
  TestingNetworkWaiter network_waiter(&mock_waiter, wake_up_factory);
  MockWriteCallback callback;

  EXPECT_CALL(mock_waiter, StopWatchingUdpSocketWritable(&socket)).Times(1);
  network_waiter.CancelWriteAll(&socket);
  EXPECT_EQ(network_waiter.IsMappedWrite(&socket), nullptr);

  EXPECT_CALL(mock_waiter, WatchUdpSocketWritable(&socket)).Times(1);
  network_waiter.WriteAll(&socket, &callback);

  EXPECT_CALL(mock_waiter, StopWatchingUdpSocketWritable(&socket)).Times(1);
  network_waiter.CancelWriteAll(&socket);
  EXPECT_EQ(network_waiter.IsMappedWrite(&socket), nullptr);
}

TEST(NetworkWaiterTest, WaitErrorOnNoTaskRunner) {
  MockEventWaiter mock_waiter;
  MockUdpSocket socket;
  std::function<WakeUpHandler*()> wake_up_factory = []() {
    return new MockWakeUpHandler();
  };
  TestingNetworkWaiter network_waiter(&mock_waiter, wake_up_factory);
  auto timeout = Clock::duration(0);

  EXPECT_EQ(network_waiter.Wait(timeout), Error::Code::kInitializationFailure);
}

TEST(NetworkWaiterTest, WaitBubblesUpWaitForEventsErrors) {
  MockEventWaiter mock_waiter;
  MockUdpSocket socket;
  std::function<WakeUpHandler*()> wake_up_factory = []() {
    return new MockWakeUpHandler();
  };
  TestingNetworkWaiter network_waiter(&mock_waiter, wake_up_factory);
  MockTaskRunner task_runner;
  network_waiter.SetTaskRunner(&task_runner);
  auto timeout = Clock::duration(0);

  auto result = ErrorOr<Events>{Error::Code::kAgain};
  EXPECT_CALL(mock_waiter, WaitForEvents(timeout)).WillOnce(Return(result));
  EXPECT_EQ(network_waiter.Wait(timeout), Error::Code::kAgain);

  result = ErrorOr<Events>{Error::Code::kNoItemFound};
  EXPECT_CALL(mock_waiter, WaitForEvents(timeout)).WillOnce(Return(result));
  EXPECT_EQ(network_waiter.Wait(timeout), Error::Code::kNoItemFound);
}

TEST(NetworkWaiterTest, WaitReturnsSuccessfulOnNoEvents) {
  MockEventWaiter mock_waiter;
  MockUdpSocket socket;
  std::function<WakeUpHandler*()> wake_up_factory = []() {
    return new MockWakeUpHandler();
  };
  TestingNetworkWaiter network_waiter(&mock_waiter, wake_up_factory);
  MockTaskRunner task_runner;
  network_waiter.SetTaskRunner(&task_runner);
  auto timeout = Clock::duration(0);
  Events results_of_wait;

  EXPECT_CALL(mock_waiter, WaitForEvents(timeout))
      .WillOnce(Return(results_of_wait));
  EXPECT_EQ(network_waiter.Wait(timeout), Error::Code::kNone);
}

TEST(NetworkWaiterTest, WaitSuccessfulReadAndCallCallback) {
  MockEventWaiter mock_waiter;
  MockUdpSocket socket;
  std::function<WakeUpHandler*()> wake_up_factory = []() {
    return new MockWakeUpHandler();
  };
  TestingNetworkWaiter network_waiter(&mock_waiter, wake_up_factory);
  MockTaskRunner task_runner;
  network_waiter.SetTaskRunner(&task_runner);
  auto timeout = Clock::duration(0);
  UdpReadCallback::Packet packet;

  EXPECT_CALL(mock_waiter, WatchUdpSocketReadable(&socket)).Times(1);
  UdpSocketReadableEvent read_event{&socket};
  Events results_of_wait;
  results_of_wait.udp_readable_events.push_back(read_event);
  MockReadCallback mock_callback;
  network_waiter.ReadAll(&socket, &mock_callback);

  EXPECT_CALL(mock_waiter, WaitForEvents(timeout))
      .WillOnce(Return(results_of_wait));
  EXPECT_CALL(network_waiter, ReadData(&socket)).WillOnce(Return(&packet));
  EXPECT_CALL(mock_callback, OnRead(&packet, &network_waiter)).Times(1);
  EXPECT_EQ(network_waiter.Wait(timeout), Error::Code::kNone);
  EXPECT_EQ(task_runner.tasks_posted, uint32_t{1});
}

TEST(NetworkWaiterTest, WaitSuccessfulReadAndNoCallback) {
  MockEventWaiter mock_waiter;
  MockUdpSocket socket;
  std::function<WakeUpHandler*()> wake_up_factory = []() {
    return new MockWakeUpHandler();
  };
  TestingNetworkWaiter network_waiter(&mock_waiter, wake_up_factory);
  MockTaskRunner task_runner;
  network_waiter.SetTaskRunner(&task_runner);
  auto timeout = Clock::duration(0);

  UdpSocketReadableEvent read_event{&socket};
  Events results_of_wait;
  results_of_wait.udp_readable_events.push_back(read_event);

  EXPECT_CALL(mock_waiter, WaitForEvents(timeout))
      .WillOnce(Return(results_of_wait));
  EXPECT_EQ(network_waiter.Wait(timeout), Error::Code::kSocketReadFailure);
}

TEST(NetworkWaiterTest, WaitSuccessfulWriteAndCallCallback) {
  MockEventWaiter mock_waiter;
  MockUdpSocket socket;
  std::function<WakeUpHandler*()> wake_up_factory = []() {
    return new MockWakeUpHandler();
  };
  TestingNetworkWaiter network_waiter(&mock_waiter, wake_up_factory);
  MockTaskRunner task_runner;
  network_waiter.SetTaskRunner(&task_runner);
  auto timeout = Clock::duration(0);

  EXPECT_CALL(mock_waiter, WatchUdpSocketWritable(&socket)).Times(1);
  UdpSocketWritableEvent write_event{&socket};
  Events results_of_wait;
  results_of_wait.udp_writable_events.push_back(write_event);
  MockWriteCallback mock_callback;
  network_waiter.WriteAll(&socket, &mock_callback);

  EXPECT_CALL(mock_waiter, WaitForEvents(timeout))
      .WillOnce(Return(results_of_wait));
  EXPECT_CALL(mock_callback, OnWrite(&network_waiter)).Times(1);
  EXPECT_EQ(network_waiter.Wait(timeout), Error::Code::kNone);
  EXPECT_EQ(task_runner.tasks_posted, uint32_t{1});
}

TEST(NetworkWaiterTest, WaitSuccessfulWriteAndNoCallback) {
  MockEventWaiter mock_waiter;
  MockUdpSocket socket;
  std::function<WakeUpHandler*()> wake_up_factory = []() {
    return new MockWakeUpHandler();
  };
  TestingNetworkWaiter network_waiter(&mock_waiter, wake_up_factory);
  MockTaskRunner task_runner;
  network_waiter.SetTaskRunner(&task_runner);
  auto timeout = Clock::duration(0);

  UdpSocketWritableEvent write_event{&socket};
  Events results_of_wait;
  results_of_wait.udp_writable_events.push_back(write_event);

  EXPECT_CALL(mock_waiter, WaitForEvents(timeout))
      .WillOnce(Return(results_of_wait));
  EXPECT_EQ(network_waiter.Wait(timeout), Error::Code::kSocketSendFailure);
}

}  // namespace platform
}  // namespace openscreen
