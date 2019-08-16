// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/test/mock_udp_socket.h"

namespace openscreen {
namespace platform {

MockUdpSocket::MockUdpSocket(TaskRunner* task_runner,
                             Client* client,
                             Version version)
    : UdpSocket(task_runner, client), version_(version) {}

bool MockUdpSocket::IsIPv4() const {
  return version_ == UdpSocket::Version::kV4;
}

bool MockUdpSocket::IsIPv6() const {
  return version_ == UdpSocket::Version::kV6;
}

IPEndpoint MockUdpSocket::GetLocalEndpoint() const {
  return IPEndpoint{};
}

// static
std::unique_ptr<MockUdpSocketDefaults> MockUdpSocket::CreateDefault(
    UdpSocket::Version version) {
  return std::make_unique<MockUdpSocketDefaults>(version);
}

MockUdpSocketDefaults::MockUdpSocketDefaults(UdpSocket::Version version)
    : clock_(Clock::now()),
      task_runner_(&clock_),
      socket_(&task_runner_, &client_, version) {}

}  // namespace platform
}  // namespace openscreen
