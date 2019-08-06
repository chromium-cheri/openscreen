// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/udp_socket.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/test/fake_network_runner.h"

namespace openscreen {
namespace platform {
namespace {
class PartiallyMockedUdpSocket : public UdpSocket {
 public:
  explicit PartiallyMockedUdpSocket(NetworkRunner* network_runner,
                                    Version version = Version::kV4)
      : UdpSocket(network_runner), version_(version) {}
  ~PartiallyMockedUdpSocket() override = default;

  bool IsIPv4() const override { return version_ == UdpSocket::Version::kV4; }
  bool IsIPv6() const override { return version_ == UdpSocket::Version::kV6; }

  MOCK_METHOD0(Bind, Error());
  MOCK_METHOD1(SetMulticastOutboundInterface, Error(NetworkInterfaceIndex));
  MOCK_METHOD2(JoinMulticastGroup,
               Error(const IPAddress&, NetworkInterfaceIndex));
  MOCK_METHOD0(ReceiveMessage, Error());
  MOCK_METHOD3(SendMessage, Error(const void*, size_t, const IPEndpoint&));
  MOCK_METHOD1(SetDscp, Error(DscpMode));

  Error SetCallback(UdpReadCallback* callback) {
    return set_read_callback(callback);
  }
  Error ClearCallback() { return clear_read_callback(); }

  using UdpSocket::PostReadData;

 private:
  Version version_;
};

class FakeCallback : public UdpReadCallback {
  void OnRead(UdpPacket packet, NetworkRunner* network_runner) override {}
};
}  // namespace

TEST(UdpSocketTest, TestCallbackAssignment) {
  FakeNetworkRunner network_runner;
  PartiallyMockedUdpSocket socket(&network_runner);
  FakeCallback callback;

  EXPECT_EQ(socket.SetCallback(&callback), Error::Code::kNone);
  EXPECT_EQ(socket.SetCallback(&callback), Error::Code::kIOFailure);

  EXPECT_EQ(socket.ClearCallback(), Error::Code::kNone);
  EXPECT_EQ(socket.ClearCallback(), Error::Code::kNotRunning);
}

TEST(UdpSocketTest, TestCallbackOnlyCalledWhenUnset) {
  FakeNetworkRunner network_runner;
  PartiallyMockedUdpSocket socket(&network_runner);

  EXPECT_EQ(network_runner.TaskCount(), uint32_t{0});
  UdpPacket packet;
  socket.PostReadData(std::move(packet));
  EXPECT_EQ(network_runner.TaskCount(), uint32_t{0});

  FakeCallback callback;
  UdpPacket packet2;
  socket.PostReadData(std::move(packet2));
  EXPECT_EQ(socket.SetCallback(&callback), Error::Code::kNone);
  EXPECT_EQ(network_runner.TaskCount(), uint32_t{0});

  EXPECT_EQ(socket.ClearCallback(), Error::Code::kNone);
}

}  // namespace platform
}  // namespace openscreen
