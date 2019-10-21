// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CHANNEL_TEST_FAKE_CAST_SOCKET_H_
#define CAST_COMMON_CHANNEL_TEST_FAKE_CAST_SOCKET_H_

#include "cast/common/channel/cast_socket.h"
#include "gmock/gmock.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"
#include "platform/test/mock_tls_connection.h"

namespace cast {
namespace channel {

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

}  // namespace channel
}  // namespace cast

#endif  // CAST_COMMON_CHANNEL_TEST_FAKE_CAST_SOCKET_H_
