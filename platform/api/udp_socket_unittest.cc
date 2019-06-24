// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/udp_socket.h"

#include "gtest/gtest.h"

namespace openscreen {
namespace platform {

class TestUdpSocket : public UdpSocket {
 public:
  TestUdpSocket() = default;
};

// Under some conditions, calling a callback can result in an exception
// "terminate called after throwing an instance of 'std::bad_function_call'"
// which will then crash the running code. This test ensures that deleting a
// new, unmodified UDP Socket object doesn't hit this edge case.
TEST(UdpSocketTest, TestDeletionWithoutCallbackSet) {
  UdpSocket* socket = new TestUdpSocket();
  delete socket;
}

TEST(UdpSocketTest, TestCallbackCalledOnDeletion) {
  UdpSocket* socket = new TestUdpSocket();
  bool is_called = false;
  std::function<void(UdpSocket*)> callback = [&is_called](UdpSocket* socket) {
    is_called = true;
  };
  socket->SetDeletionCallback(callback);

  EXPECT_FALSE(is_called);
  delete socket;

  EXPECT_TRUE(is_called);
}

}  // namespace platform
}  // namespace openscreen
