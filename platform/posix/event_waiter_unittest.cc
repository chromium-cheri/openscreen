// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/posix/event_waiter.h"

#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {
namespace platform {

using namespace ::testing;
using ::testing::_;
using ::testing::Invoke;

// NOTE: Due to a gmock bug, WakeUpHandlerPosix cannot be mocked. Creating a
// mock class that extends this class causes a SegFault when delete is called on
// it as part of the destructor of EventWaiterPosix. Doing so with the base
// class does not, so that's the approach that was taken for these tests.

// Mock SocketHandler to let us avoid the FileHandles used in the production
// version of this class.
class MockSocketHandler : public SocketHandler {
 public:
  MOCK_METHOD2(Watch, void(int, bool));
  MOCK_METHOD1(IsChanged, bool(int));
};

// Mock socket so we can test without actually using real sockets.
class MockUdpSocket : public UdpSocketPosix {
 public:
  MockUdpSocket(int fd) : UdpSocketPosix(fd, Version::kV4) {}
};

// Class extending EventWaiterPosix to give access to private variables, so we
// can verify that they are behaving as expected.
class TestingEventWaiterPosix : public EventWaiterPosix {
 public:
  TestingEventWaiterPosix(WakeUpHandler* handler) : EventWaiterPosix(handler) {}
  ~TestingEventWaiterPosix() = default;
  MOCK_METHOD3(WaitForSockets,
               Error(Clock::duration, SocketHandler*, SocketHandler*));

  bool IsReadableWatched(UdpSocketPosix* socket) {
    return IsInVector(this->read_sockets, socket);
  }

  bool IsWritableWatched(UdpSocketPosix* socket) {
    return IsInVector(this->write_sockets, socket);
  }

 private:
  bool IsInVector(const std::vector<UdpSocketPosix*>& vect,
                  UdpSocketPosix* socket) {
    for (const auto* s : vect) {
      if (s->fd == socket->fd) {
        return true;
      }
    }

    return false;
  }
};

TEST(EventWaiterPosix, UdpSocketReadable) {
  WakeUpHandler* wake_up_handler = new WakeUpHandlerPosix(12345, 0);
  TestingEventWaiterPosix event_waiter(wake_up_handler);
  MockUdpSocket socket(123);

  EXPECT_FALSE(event_waiter.IsReadableWatched(&socket));
  event_waiter.WatchUdpSocketReadable(&socket);
  EXPECT_TRUE(event_waiter.IsReadableWatched(&socket));
  event_waiter.StopWatchingUdpSocketReadable(&socket);
  EXPECT_FALSE(event_waiter.IsReadableWatched(&socket));
}

TEST(EventWaiterPosix, UdpSocketWritable) {
  WakeUpHandler* wake_up_handler = new WakeUpHandlerPosix(12345, 0);
  TestingEventWaiterPosix event_waiter(wake_up_handler);
  MockUdpSocket socket(123);

  EXPECT_FALSE(event_waiter.IsWritableWatched(&socket));
  event_waiter.WatchUdpSocketWritable(&socket);
  EXPECT_TRUE(event_waiter.IsWritableWatched(&socket));
  event_waiter.StopWatchingUdpSocketWritable(&socket);
  EXPECT_FALSE(event_waiter.IsWritableWatched(&socket));
}

TEST(EventWaiterPosix, WaitForEventsErrorBubblesUp) {
  constexpr int wake_up_fd = 12345;
  WakeUpHandler* wake_up_handler = new WakeUpHandlerPosix(wake_up_fd, 0);
  TestingEventWaiterPosix event_waiter(wake_up_handler);
  MockUdpSocket socket(123);

  MockSocketHandler read_handler;
  MockSocketHandler write_handler;
  auto timeout = Clock::duration(1);

  EXPECT_CALL(read_handler, Watch(wake_up_fd, false)).Times(1);
  EXPECT_CALL(event_waiter,
              WaitForSockets(timeout, &read_handler, &write_handler))
      .WillOnce(Return(Error::Code::kIOFailure));
  auto result =
      event_waiter.WaitForEvents(timeout, &read_handler, &write_handler);
  EXPECT_TRUE(result.is_error());
  EXPECT_EQ(result.error().code(), Error::Code::kIOFailure);
}

TEST(EventWaiterPosix, WaitForEventsHasReadReturnsTrue) {
  constexpr int wake_up_fd = 12345;
  constexpr int socket_fd = 123;
  WakeUpHandler* wake_up_handler = new WakeUpHandlerPosix(12345, 0);
  TestingEventWaiterPosix event_waiter(wake_up_handler);
  MockUdpSocket socket(socket_fd);

  MockSocketHandler read_handler;
  MockSocketHandler write_handler;
  auto timeout = Clock::duration(1);
  event_waiter.WatchUdpSocketReadable(&socket);

  EXPECT_CALL(read_handler, Watch(wake_up_fd, false)).Times(1);
  EXPECT_CALL(read_handler, Watch(socket_fd, true)).Times(1);
  EXPECT_CALL(event_waiter,
              WaitForSockets(timeout, &read_handler, &write_handler))
      .WillOnce(Return(Error::Code::kNone));
  EXPECT_CALL(read_handler, IsChanged(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(write_handler, IsChanged(_)).WillRepeatedly(Return(true));
  auto result =
      event_waiter.WaitForEvents(timeout, &read_handler, &write_handler);
  EXPECT_FALSE(result.is_error());
  EXPECT_EQ(result.value().udp_readable_events.size(), size_t{1});
  EXPECT_EQ(result.value().udp_writable_events.size(), size_t{0});
  EXPECT_EQ(result.value().udp_readable_events[0].socket, &socket);
}

TEST(EventWaiterPosix, WaitForEventsHasWriteReturnsTrue) {
  constexpr int wake_up_fd = 12345;
  constexpr int socket_fd = 123;
  WakeUpHandler* wake_up_handler = new WakeUpHandlerPosix(12345, 0);
  TestingEventWaiterPosix event_waiter(wake_up_handler);
  MockUdpSocket socket(socket_fd);

  MockSocketHandler read_handler;
  MockSocketHandler write_handler;
  auto timeout = Clock::duration(1);
  event_waiter.WatchUdpSocketWritable(&socket);

  EXPECT_CALL(read_handler, Watch(wake_up_fd, false)).Times(1);
  EXPECT_CALL(write_handler, Watch(socket_fd, true)).Times(1);
  EXPECT_CALL(event_waiter,
              WaitForSockets(timeout, &read_handler, &write_handler))
      .WillOnce(Return(Error::Code::kNone));
  EXPECT_CALL(read_handler, IsChanged(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(write_handler, IsChanged(_)).WillRepeatedly(Return(true));
  auto result =
      event_waiter.WaitForEvents(timeout, &read_handler, &write_handler);
  EXPECT_FALSE(result.is_error());
  EXPECT_EQ(result.value().udp_readable_events.size(), size_t{0});
  EXPECT_EQ(result.value().udp_writable_events.size(), size_t{1});
  EXPECT_EQ(result.value().udp_writable_events[0].socket, &socket);
}

TEST(EventWaiterPosix, WaitForEventsHasReadWriteReturnFalse) {
  constexpr int wake_up_fd = 12345;
  constexpr int socket_fd = 123;
  WakeUpHandler* wake_up_handler = new WakeUpHandlerPosix(12345, 0);
  TestingEventWaiterPosix event_waiter(wake_up_handler);
  MockUdpSocket socket(socket_fd);

  MockSocketHandler read_handler;
  MockSocketHandler write_handler;
  auto timeout = Clock::duration(1);
  event_waiter.WatchUdpSocketWritable(&socket);
  event_waiter.WatchUdpSocketReadable(&socket);

  EXPECT_CALL(read_handler, Watch(wake_up_fd, false)).Times(1);
  EXPECT_CALL(read_handler, Watch(socket_fd, true)).Times(1);
  EXPECT_CALL(write_handler, Watch(socket_fd, true)).Times(1);
  EXPECT_CALL(event_waiter,
              WaitForSockets(timeout, &read_handler, &write_handler))
      .WillOnce(Return(Error::Code::kNone));
  EXPECT_CALL(read_handler, IsChanged(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(write_handler, IsChanged(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(read_handler, IsChanged(socket_fd)).WillOnce(Return(false));
  EXPECT_CALL(write_handler, IsChanged(socket_fd)).WillOnce(Return(false));
  auto result =
      event_waiter.WaitForEvents(timeout, &read_handler, &write_handler);
  EXPECT_FALSE(result.is_error());
  EXPECT_EQ(result.value().udp_readable_events.size(), size_t{0});
  EXPECT_EQ(result.value().udp_writable_events.size(), size_t{0});
}

TEST(EventWaiterPosix, ValidateTimevalConversion) {
  auto timespan = Clock::duration::zero();
  auto timeval =
      SocketHandlerPosix::ToTimeval(timespan) EXPECT_EQ(timeval.tv_sec, 0);
  EXPECT_EQ(timeval.tv_usec, 0);

  auto timespan = Clock::duration(1000000);
  auto timeval =
      SocketHandlerPosix::ToTimeval(timespan) EXPECT_EQ(timeval.tv_sec, 1);
  EXPECT_EQ(timeval.tv_usec, 0);

  auto timespan = Clock::duration(1000000 - 1);
  auto timeval =
      SocketHandlerPosix::ToTimeval(timespan) EXPECT_EQ(timeval.tv_sec, 0);
  EXPECT_EQ(timeval.tv_usec, 1000000 - 1);

  auto timespan = Clock::duration(1);
  auto timeval =
      SocketHandlerPosix::ToTimeval(timespan) EXPECT_EQ(timeval.tv_sec, 0);
  EXPECT_EQ(timeval.tv_usec, 1);

  auto timespan = Clock::duration(100000010);
  auto timeval =
      SocketHandlerPosix::ToTimeval(timespan) EXPECT_EQ(timeval.tv_sec, 100);
  EXPECT_EQ(timeval.tv_usec, 10);
}

}  // namespace platform
}  // namespace openscreen
