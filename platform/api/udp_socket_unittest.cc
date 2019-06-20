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

TEST(UdpSocketTest, TestCallback) {
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
