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

  void StopListening(StreamSocketPosix* socket) override {
    TlsDataRouterPosix::OnSocketDestroyed(socket, true);
  }
};

class MockObserver : public StreamSocketPosix::Observer {
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
  StreamSocketPosix local_socket(IPAddress::Version::kV4);
  MockObserver observer;

  ASSERT_FALSE(network_manager.IsSocketWatched(&local_socket));

  auto* socket =
      network_manager.StartListening(std::move(local_socket), &observer);
  ASSERT_TRUE(network_manager.IsSocketWatched(socket));

  network_manager.StopListening(socket);
  ASSERT_FALSE(network_manager.IsSocketWatched(socket));

  network_manager.StopListening(socket);
  ASSERT_FALSE(network_manager.IsSocketWatched(socket));
}

}  // namespace platform
}  // namespace openscreen
