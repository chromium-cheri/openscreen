// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/test/mock_udp_socket.h"

namespace openscreen {
namespace platform {

MockUdpSocket::MockUdpSocket(TaskRunner* task_runner,
                             Client* client,
                             Version version)
    : UdpSocket(task_runner, client, true), version_(version) {}

// Ensure that the destructors for the unique_ptr objects are called in
// the correct order to avoid OSP_CHECK failures.
MockUdpSocket::~MockUdpSocket() {
  fake_task_runner_.reset();
  fake_clock_.reset();
  fake_client_.reset();
}

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
std::unique_ptr<MockUdpSocket> MockUdpSocket::CreateDefault(
    UdpSocket::Version version) {
  std::unique_ptr<FakeClock> clock = std::make_unique<FakeClock>(Clock::now());
  std::unique_ptr<FakeTaskRunner> task_runner =
      std::make_unique<FakeTaskRunner>(clock.get());
  std::unique_ptr<UdpSocket::Client> client =
      std::unique_ptr<UdpSocket::Client>(new MockUdpSocket::MockClient);

  std::unique_ptr<MockUdpSocket> socket =
      std::make_unique<MockUdpSocket>(task_runner.get(), client.get(), version);
  socket->fake_clock_.swap(clock);
  socket->fake_client_.swap(client);
  socket->fake_task_runner_.swap(task_runner);

  return socket;
}

}  // namespace platform
}  // namespace openscreen
