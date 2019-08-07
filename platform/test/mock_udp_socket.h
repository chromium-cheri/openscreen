// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_TEST_MOCK_UDP_SOCKET_H_
#define PLATFORM_TEST_MOCK_UDP_SOCKET_H_

#include <algorithm>
#include <future>
#include <memory>

#include "gmock/gmock.h"
#include "platform/api/udp_socket.h"

namespace openscreen {
namespace platform {

class MockUdpSocket : public UdpSocket {
 public:
  explicit MockUdpSocket(NetworkRunner* network_runner,
                         Version version = Version::kV4);
  ~MockUdpSocket() override = default;

  bool IsIPv4() const override;
  bool IsIPv6() const override;

  MOCK_METHOD1(Bind, std::future<void>(Callback));
  MOCK_METHOD2(SetMulticastOutboundInterface,
               std::future<void>(Callback, NetworkInterfaceIndex));
  MOCK_METHOD3(JoinMulticastGroup,
               std::future<void>(Callback,
                                 const IPAddress&,
                                 NetworkInterfaceIndex));
  MOCK_METHOD2(SetDscp, std::future<void>(Callback, DscpMode));
  MOCK_METHOD0(ReceiveMessage, ErrorOr<UdpPacket>());
  MOCK_METHOD3(SendMessage, Error(const void*, size_t, const IPEndpoint&));

 private:
  Version version_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_TEST_MOCK_UDP_SOCKET_H_
