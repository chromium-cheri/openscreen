// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CHANNEL_TEST_FAKE_CAST_SOCKET_H_
#define CAST_COMMON_CHANNEL_TEST_FAKE_CAST_SOCKET_H_

#include <memory>

#include "cast/common/channel/cast_socket.h"
#include "gmock/gmock.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"
#include "platform/test/mock_tls_connection.h"

namespace cast {
namespace channel {

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

using openscreen::IPEndpoint;
using openscreen::platform::FakeClock;
using openscreen::platform::FakeTaskRunner;
using openscreen::platform::MockTlsConnection;
using openscreen::platform::TaskRunner;
using openscreen::platform::TlsConnection;

class MockCastSocketClient final : public CastSocket::Client {
 public:
  ~MockCastSocketClient() override = default;

  MOCK_METHOD(void, OnError, (CastSocket * socket, Error error), (override));
  MOCK_METHOD(void,
              OnMessage,
              (CastSocket * socket, CastMessage message),
              (override));
};

struct FakeCastSocket {
  FakeClock clock{openscreen::platform::Clock::now()};
  FakeTaskRunner task_runner{&clock};
  IPEndpoint local{{10, 0, 1, 7}, 1234};
  IPEndpoint remote{{10, 0, 1, 9}, 4321};
  std::unique_ptr<MockTlsConnection> moved_connection{
      new MockTlsConnection(&task_runner, local, remote)};
  MockTlsConnection* connection{moved_connection.get()};
  MockCastSocketClient mock_client;
  CastSocket socket{std::move(moved_connection), &mock_client, 1};
};

struct FakeCastSocketPair {
  FakeCastSocketPair() {
    ON_CALL(*connection, Write(_, _))
        .WillByDefault(Invoke([this](const void* data, size_t len) {
          peer_connection->OnRead(std::vector<uint8_t>(
              reinterpret_cast<const uint8_t*>(data),
              reinterpret_cast<const uint8_t*>(data) + len));
        }));
    ON_CALL(*peer_connection, Write(_, _))
        .WillByDefault(Invoke([this](const void* data, size_t len) {
          connection->OnRead(std::vector<uint8_t>(
              reinterpret_cast<const uint8_t*>(data),
              reinterpret_cast<const uint8_t*>(data) + len));
        }));
  }
  ~FakeCastSocketPair() = default;

  FakeClock clock{openscreen::platform::Clock::now()};
  FakeTaskRunner task_runner{&clock};
  IPEndpoint local{{10, 0, 1, 7}, 1234};
  IPEndpoint remote{{10, 0, 1, 9}, 4321};

  std::unique_ptr<NiceMock<MockTlsConnection>> moved_connection{
      new NiceMock<MockTlsConnection>(&task_runner, local, remote)};
  NiceMock<MockTlsConnection>* connection{moved_connection.get()};
  MockCastSocketClient mock_client;
  std::unique_ptr<CastSocket> socket{
      new CastSocket(std::move(moved_connection), &mock_client, 1)};

  std::unique_ptr<NiceMock<MockTlsConnection>> moved_connection2{
      new NiceMock<MockTlsConnection>(&task_runner, remote, local)};
  NiceMock<MockTlsConnection>* peer_connection{moved_connection2.get()};
  MockCastSocketClient mock_peer_client;
  CastSocket peer_socket{std::move(moved_connection2), &mock_peer_client, 2};
};

}  // namespace channel
}  // namespace cast

#endif  // CAST_COMMON_CHANNEL_TEST_FAKE_CAST_SOCKET_H_
