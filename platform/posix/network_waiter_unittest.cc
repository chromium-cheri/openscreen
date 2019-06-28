// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/posix/network_waiter.h"

#include <sys/socket.h>

#include <chrono>
#include <iostream>
#include <thread>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/api/udp_socket.h"

namespace openscreen {
namespace platform {

using namespace ::testing;
using ::testing::_;

TEST(NetworkWaiterPosixTest, ValidateTimevalConversion) {
  auto timespan = Clock::duration::zero();
  auto timeval = NetworkWaiterPosix::ToTimeval(timespan);
  EXPECT_EQ(timeval.tv_sec, 0);
  EXPECT_EQ(timeval.tv_usec, 0);

  timespan = Clock::duration(1000000);
  timeval = NetworkWaiterPosix::ToTimeval(timespan);
  EXPECT_EQ(timeval.tv_sec, 1);
  EXPECT_EQ(timeval.tv_usec, 0);

  timespan = Clock::duration(1000000 - 1);
  timeval = NetworkWaiterPosix::ToTimeval(timespan);
  EXPECT_EQ(timeval.tv_sec, 0);
  EXPECT_EQ(timeval.tv_usec, 1000000 - 1);

  timespan = Clock::duration(1);
  timeval = NetworkWaiterPosix::ToTimeval(timespan);
  EXPECT_EQ(timeval.tv_sec, 0);
  EXPECT_EQ(timeval.tv_usec, 1);

  timespan = Clock::duration(100000010);
  timeval = NetworkWaiterPosix::ToTimeval(timespan);
  EXPECT_EQ(timeval.tv_sec, 100);
  EXPECT_EQ(timeval.tv_usec, 10);
}

TEST(NetworkWaiterPosixTest, DeleteSocketDuringWait) {
  // Create sockets for testing.
  int pipefds[2];
  EXPECT_NE(pipe(pipefds), -1);
  int write_fd = pipefds[1];
  UdpSocketPosix read_socket(pipefds[0], UdpSocket::Version::kV4);

  auto* deletable_socket =
      UdpSocket::Create(UdpSocket::Version::kV4).MoveValue().release();

  // Schedule thread cancellation.
  std::atomic_bool is_deleted(false);
  std::atomic_bool is_waiting(false);

  std::function<void()> canceler = [deletable_socket, &is_waiting, &is_deleted,
                                    &write_fd]() {
    while (!is_waiting.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    delete deletable_socket;

    // Break out of the select(...) call.
    is_deleted.store(true);
    uint8_t byte = uint8_t{0x1};
    write(write_fd, &byte, sizeof(byte));
  };
  std::thread cancel_thread(canceler);

  // Execute the wait call.
  NetworkWaiterPosix waiter;
  std::vector<UdpSocket*> sockets = {deletable_socket, &read_socket};
  auto timeout = std::chrono::milliseconds(1000);
  while (true) {
    is_waiting.store(true);
    auto error = waiter.AwaitSocketsReadable(sockets, timeout);
    if (is_deleted.load()) {
      break;
    }
  }

  // Whether or not an error is detected by Select seems nondeterminist, but if
  // one is it gets converted to a kIOFailure. So here just make sure that a
  // failure doesn't cause an exception to be thrown.

  cancel_thread.join();
}

}  // namespace platform
}  // namespace openscreen
