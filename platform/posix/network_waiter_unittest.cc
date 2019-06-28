// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/posix/network_waiter.h"

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
  // Create a socket and call wait on it on thread 1.
  auto* socket1 =
      UdpSocket::Create(UdpSocket::Version::kV4).MoveValue().release();
  auto socket2 = UdpSocket::Create(UdpSocket::Version::kV4).MoveValue();

  NetworkWaiterPosix waiter;
  std::vector<UdpSocket*> sockets = {socket1, socket2.get()};

  std::atomic_bool is_deleted(false);
  std::atomic_bool is_waiting(false);

  std::function<void()> canceler = [socket1, &is_waiting, &is_deleted]() {
    auto timeout = std::chrono::milliseconds(25);
    while (!is_waiting.load()) {
      std::this_thread::sleep_for(timeout);
    }

    delete socket1;

    is_deleted.store(true);
  };

  std::thread cancel_thread(canceler);

  auto timeout = std::chrono::milliseconds(25);
  while (true) {
    auto error = waiter.AwaitSocketsReadable(sockets, timeout);
    if (is_deleted.load()) {
      break;
    }

    EXPECT_TRUE(error.is_error());
    EXPECT_EQ(error.error().code(), Error::Code::kAgain);
    is_waiting.store(true);
  }

  // Whether or not an error is detected by Select seems nondeterminist, but if
  // one is it gets converted to a kIOFailure. So here just make sure that a
  // failure doesn't cause an exception to be thrown.

  cancel_thread.join();
}

}  // namespace platform
}  // namespace openscreen
