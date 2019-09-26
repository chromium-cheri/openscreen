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

using testing::_;

class TestingDataRouter : public TlsDataRouterPosix {
 public:
  TestingDataRouter(NetworkReaderWriterPosix* reader_writer,
                    SocketHandleWaiter* waiter)
      : TlsDataRouterPosix(reader_writer, waiter) {}

  using TlsDataRouterPosix::IsSocketWatched;

  void OnSocketDestroyed(StreamSocketPosix* socket) override {
    TlsDataRouterPosix::OnSocketDestroyed(socket, true);
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

class MockNetworkReaderWriter : public NetworkReaderWriterPosix {
 public:
  MOCK_METHOD1(RegisterProvider, void(Provider*));
  MOCK_METHOD1(DeregisterProvider, void(Provider*));
};

TEST(TlsNetworkingManagerPosixTest, SocketsWatchedCorrectly) {
  MockNetworkReaderWriter reader_writer;
  EXPECT_CALL(reader_writer, RegisterProvider(_)).Times(1);
  EXPECT_CALL(reader_writer, DeregisterProvider(_)).Times(1);
  MockNetworkWaiter network_waiter;
  TestingDataRouter network_manager(&reader_writer, &network_waiter);
  StreamSocketPosix socket(IPAddress::Version::kV4);
  MockObserver observer;

  ASSERT_FALSE(network_manager.IsSocketWatched(&socket));

  network_manager.RegisterSocketObserver(&socket, &observer);
  ASSERT_TRUE(network_manager.IsSocketWatched(&socket));

  network_manager.DeregisterSocketObserver(&socket);
  ASSERT_FALSE(network_manager.IsSocketWatched(&socket));

  network_manager.RegisterSocketObserver(&socket, &observer);
  ASSERT_TRUE(network_manager.IsSocketWatched(&socket));

  network_manager.OnSocketDestroyed(&socket);
  ASSERT_FALSE(network_manager.IsSocketWatched(&socket));
}

}  // namespace platform
}  // namespace openscreen
