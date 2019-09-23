// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/tls_networking_manager_posix.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/base/ip_address.h"
#include "platform/impl/stream_socket_posix.h"

namespace openscreen {
namespace platform {

class TestingNetworkManager : public TlsNetworkingManagerPosix {
 public:
  TestingNetworkManager(NetworkWaiter* waiter)
      : TlsNetworkingManagerPosix(waiter) {}

  using TlsNetworkingManagerPosix::IsSocketMapped;

  void OnSocketDestroyed(StreamSocketPosix* socket) override {
    TlsNetworkingManagerPosix::OnSocketDestroyed(socket, true);
  }
};

class MockObserver : public TlsNetworkingManagerPosix::SocketObserver {
  MOCK_METHOD1(OnConnectionPending, void(StreamSocketPosix*));
};

class MockNetworkWaiter final : public NetworkWaiter {
 public:
  MOCK_METHOD2(
      AwaitSocketsReadable,
      ErrorOr<std::vector<SocketHandleRef>>(const std::vector<SocketHandleRef>&,
                                            const Clock::duration&));
};

TEST(TlsNetworkingManagerPosixTest, SocketsWatchedCorrectly) {
  MockNetworkWaiter network_waiter;
  TestingNetworkManager network_manager(&network_waiter);
  StreamSocketPosix socket(IPAddress::Version::kV4);
  MockObserver observer;

  ASSERT_FALSE(network_manager.IsSocketMapped(&socket));

  network_manager.RegisterListener(&socket, &observer);
  ASSERT_TRUE(network_manager.IsSocketMapped(&socket));

  network_manager.DeregisterListener(&socket);
  ASSERT_FALSE(network_manager.IsSocketMapped(&socket));

  network_manager.RegisterListener(&socket, &observer);
  ASSERT_TRUE(network_manager.IsSocketMapped(&socket));

  network_manager.OnSocketDestroyed(&socket);
  ASSERT_FALSE(network_manager.IsSocketMapped(&socket));
}

}  // namespace platform
}  // namespace openscreen
