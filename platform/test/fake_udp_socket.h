// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_TEST_FAKE_UDP_SOCKET_H_
#define PLATFORM_TEST_FAKE_UDP_SOCKET_H_

#include <algorithm>
#include <memory>

#include "platform/api/logging.h"
#include "platform/api/udp_socket.h"

namespace openscreen {
namespace platform {

class FakeUdpSocket : public UdpSocket {
 public:
  FakeUdpSocket() = default;
  ~FakeUdpSocket() = default;

  MOCK_METHOD0(IsIPv4, bool());
  MOCK_METHOD0(IsIPv6, bool());
  MOCK_METHOD1(Bind, Error(const IPEndpoint&));
  MOCK_METHOD1(SetMulticastOutboundInterface, Error(NetworkInterfaceIndex));
  MOCK_METHOD2(JoinMulticastGroup,
               Error(const IPAddress&, NetworkInterfaceIndex));
  MOCK_METHOD4(ReceiveMessage,
               ErrorOr<size_t>(void*, size_t, IPEndpoint*, IPEndpoint*));
  MOCK_METHOD3(SendMessage, Error(const void*, size_t, const IPEndpoint&));
  MOCK_METHOD1(SetDscp, Error(DscpMode));
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_TEST_FAKE_UDP_SOCKET_H_
