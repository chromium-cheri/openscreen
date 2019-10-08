// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/tls_data_router_posix.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/base/ip_address.h"
#include "platform/impl/stream_socket_posix.h"

namespace openscreen {
namespace platform {

class TestingDataRouter : public TlsDataRouterPosix {
 public:
  TestingDataRouter(SocketHandleWaiter* waiter) : TlsDataRouterPosix(waiter) {}

  using TlsDataRouterPosix::IsSocketWatched;

  void DeregisterSocketObserver(StreamSocketPosix* socket) override {
    TlsDataRouterPosix::OnSocketDestroyed(socket, true);
  }

  bool AnySocketsWatched() {
    std::unique_lock<std::mutex> lock(socket_mutex_);
    return !watched_stream_sockets_.empty() && !socket_mappings_.empty();
  }
};

class MockObserver : public TestingDataRouter::SocketObserver {
  MOCK_METHOD1(OnConnectionPending, void(StreamSocketPosix*));
};

class MockNetworkWaiter final : public SocketHandleWaiter {
 public:
  MOCK_METHOD2(
      AwaitSocketsReadable,
      ErrorOr<std::vector<SocketHandleRef>>(const std::vector<SocketHandleRef>&,
                                            const Clock::duration&));
};

TEST(TlsNetworkingManagerPosixTest, SocketsWatchedCorrectly) {
  MockNetworkWaiter network_waiter;
  TestingDataRouter network_manager(&network_waiter);
  auto socket = std::make_unique<StreamSocketPosix>(IPAddress::Version::kV4);
  MockObserver observer;

  ASSERT_FALSE(network_manager.IsSocketWatched(socket.get()));

  auto* ptr =
      network_manager.RegisterSocketObserver(std::move(socket), &observer);
  ASSERT_TRUE(network_manager.IsSocketWatched(ptr));
  ASSERT_TRUE(network_manager.AnySocketsWatched());

  network_manager.DeregisterSocketObserver(ptr);
  ASSERT_FALSE(network_manager.IsSocketWatched(ptr));
  ASSERT_FALSE(network_manager.AnySocketsWatched());

  network_manager.DeregisterSocketObserver(ptr);
  ASSERT_FALSE(network_manager.IsSocketWatched(ptr));
  ASSERT_FALSE(network_manager.AnySocketsWatched());
}

}  // namespace platform
}  // namespace openscreen
