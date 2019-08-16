// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/udp_socket.h"

#include "gtest/gtest.h"
#include "platform/test/mock_udp_socket.h"

namespace openscreen {
namespace platform {
namespace {

class MockCallbacks : public UdpSocket::LifetimeObserver {
 public:
  MOCK_METHOD1(OnCreate, void(UdpSocket*));
  MOCK_METHOD1(OnDestroy, void(UdpSocket*));
};

}  // namespace

using testing::_;

TEST(UdpSocketTest, TestDeletionWithoutCallbackSet) {
  MockCallbacks callbacks;
  EXPECT_CALL(callbacks, OnCreate(_)).Times(0);
  EXPECT_CALL(callbacks, OnDestroy(_)).Times(0);
  {
    UdpSocket* socket = new MockUdpSocket(UdpSocket::Version::kV4);
    delete socket;
  }
}

TEST(UdpSocketTest, TestCallbackCalledOnDeletion) {
  MockCallbacks callbacks;
  EXPECT_CALL(callbacks, OnCreate(_)).Times(1);
  EXPECT_CALL(callbacks, OnDestroy(_)).Times(1);
  UdpSocket::SetLifetimeObserver(&callbacks);

  {
    UdpSocket* socket = new MockUdpSocket(UdpSocket::Version::kV4);
    delete socket;
  }

  UdpSocket::SetLifetimeObserver(nullptr);
}

// Tests that a UdpSocket that does not specify any address or port will
// successfully Bind(), and that the operating system will return the
// auto-assigned socket name (i.e., the local endpoint's port will not be zero).
TEST(UdpSocketTest, ResolvesLocalEndpoint_IPv4) {
  const uint8_t kIpV4AddrAny[4] = {};
  ErrorOr<UdpSocketUniquePtr> create_result =
      UdpSocket::Create(IPEndpoint{IPAddress(kIpV4AddrAny), 0});
  ASSERT_TRUE(create_result) << create_result.error();
  const auto socket = create_result.MoveValue();
  const Error bind_result = socket->Bind();
  ASSERT_TRUE(bind_result.ok()) << bind_result;
  const IPEndpoint local_endpoint = socket->GetLocalEndpoint();
  EXPECT_NE(local_endpoint.port, 0) << local_endpoint;
}

// Tests that a UdpSocket that does not specify any address or port will
// successfully Bind(), and that the operating system will return the
// auto-assigned socket name (i.e., the local endpoint's port will not be zero).
TEST(UdpSocketTest, ResolvesLocalEndpoint_IPv6) {
  const uint8_t kIpV6AddrAny[16] = {};
  ErrorOr<UdpSocketUniquePtr> create_result =
      UdpSocket::Create(IPEndpoint{IPAddress(kIpV6AddrAny), 0});
  ASSERT_TRUE(create_result) << create_result.error();
  const auto socket = create_result.MoveValue();
  const Error bind_result = socket->Bind();
  ASSERT_TRUE(bind_result.ok()) << bind_result;
  const IPEndpoint local_endpoint = socket->GetLocalEndpoint();
  EXPECT_NE(local_endpoint.port, 0) << local_endpoint;
}

}  // namespace platform
}  // namespace openscreen
