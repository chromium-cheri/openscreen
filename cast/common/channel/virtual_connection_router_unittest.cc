// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/channel/virtual_connection_router.h"

#include "cast/common/channel/cast_socket.h"
#include "cast/common/channel/proto/cast_channel.pb.h"
#include "cast/common/channel/test/fake_cast_socket.h"
#include "cast/common/channel/test/mock_cast_message_handler.h"
#include "cast/common/channel/virtual_connection_manager.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"
#include "platform/test/mock_tls_connection.h"

namespace cast {
namespace channel {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

using openscreen::IPEndpoint;
using openscreen::platform::FakeClock;
using openscreen::platform::FakeTaskRunner;
using openscreen::platform::MockTlsConnection;
using openscreen::platform::TaskRunner;
using openscreen::platform::TlsConnection;

class MockSocketErrorHandler
    : public VirtualConnectionRouter::SocketErrorHandler {
 public:
  MOCK_METHOD(void, OnClose, (CastSocket * socket), (override));
  MOCK_METHOD(void, OnError, (CastSocket * socket, Error error), (override));
};

class VirtualConnectionRouterTest : public ::testing::Test {
 public:
  void SetUp() override {
    auto socket =
        std::make_unique<CastSocket>(std::move(moved_connection_), &router_, 1);
    socket_id_ = socket->socket_id();
    router_.TakeSocket(&mock_error_handler_, std::move(socket));

    ON_CALL(*remote_connection_, Write(_, _))
        .WillByDefault(Invoke([this](const void* data, size_t len) {
          const auto* d = reinterpret_cast<const uint8_t*>(data);
          connection_->OnRead(std::vector<uint8_t>(d, d + len));
        }));
    ON_CALL(*connection_, Write(_, _))
        .WillByDefault(Invoke([this](const void* data, size_t len) {
          const auto* d = reinterpret_cast<const uint8_t*>(data);
          remote_connection_->OnRead(std::vector<uint8_t>(d, d + len));
        }));
  }

 protected:
  FakeClock clock_{openscreen::platform::Clock::now()};
  FakeTaskRunner task_runner_{&clock_};
  IPEndpoint local_{{10, 0, 1, 7}, 1234};
  IPEndpoint remote_{{10, 0, 1, 9}, 4321};

  std::unique_ptr<NiceMock<MockTlsConnection>> moved_remote_connection_{
      new NiceMock<MockTlsConnection>(&task_runner_, local_, remote_)};
  MockTlsConnection* remote_connection_{moved_remote_connection_.get()};
  MockCastSocketClient mock_remote_client_;
  CastSocket remote_socket_{std::move(moved_remote_connection_),
                            &mock_remote_client_, 1};

  std::unique_ptr<NiceMock<MockTlsConnection>> moved_connection_{
      new NiceMock<MockTlsConnection>(&task_runner_, local_, remote_)};
  MockTlsConnection* connection_{moved_connection_.get()};
  uint32_t socket_id_;

  MockSocketErrorHandler mock_error_handler_;
  MockCastMessageHandler mock_message_handler_;

  VirtualConnectionManager manager_;
  VirtualConnectionRouter router_{&manager_};
};

}  // namespace

TEST_F(VirtualConnectionRouterTest, LocalIdHandler) {
  router_.AddHandlerForLocalId("receiver-1234", &mock_message_handler_);

  CastMessage message;
  message.set_protocol_version(CastMessage_ProtocolVersion_CASTV2_1_0);
  message.set_namespace_("zrqvn");
  message.set_source_id("sender-9873");
  message.set_destination_id("receiver-1234");
  message.set_payload_type(CastMessage::STRING);
  message.set_payload_utf8("cnlybnq");
  remote_socket_.SendMessage(message);

  EXPECT_CALL(mock_message_handler_, OnMessage(_, _));
  task_runner_.RunTasksUntilIdle();

  remote_socket_.SendMessage(message);
  EXPECT_CALL(mock_message_handler_, OnMessage(_, _));
  task_runner_.RunTasksUntilIdle();

  message.set_destination_id("receiver-4321");
  remote_socket_.SendMessage(message);
  EXPECT_CALL(mock_message_handler_, OnMessage(_, _)).Times(0);
  task_runner_.RunTasksUntilIdle();
}

TEST_F(VirtualConnectionRouterTest, RemoveLocalIdHandler) {
  router_.AddHandlerForLocalId("receiver-1234", &mock_message_handler_);

  CastMessage message;
  message.set_protocol_version(CastMessage_ProtocolVersion_CASTV2_1_0);
  message.set_namespace_("zrqvn");
  message.set_source_id("sender-9873");
  message.set_destination_id("receiver-1234");
  message.set_payload_type(CastMessage::STRING);
  message.set_payload_utf8("cnlybnq");
  remote_socket_.SendMessage(message);

  EXPECT_CALL(mock_message_handler_, OnMessage(_, _));
  task_runner_.RunTasksUntilIdle();

  router_.RemoveHandlerForLocalId("receiver-1234");

  remote_socket_.SendMessage(message);
  EXPECT_CALL(mock_message_handler_, OnMessage(_, _)).Times(0);
  task_runner_.RunTasksUntilIdle();
}

TEST_F(VirtualConnectionRouterTest, SendMessage) {
  manager_.AddConnection(
      VirtualConnection{"receiver-1234", "sender-4321", socket_id_}, {});

  CastMessage message;
  message.set_protocol_version(CastMessage_ProtocolVersion_CASTV2_1_0);
  message.set_namespace_("zrqvn");
  message.set_source_id("receiver-1234");
  message.set_destination_id("sender-4321");
  message.set_payload_type(CastMessage::STRING);
  message.set_payload_utf8("cnlybnq");
  router_.SendMessage(
      VirtualConnection{"receiver-1234", "sender-4321", socket_id_},
      std::move(message));
  EXPECT_CALL(mock_remote_client_, OnMessage(_, _))
      .WillOnce(Invoke([](CastSocket* socket, CastMessage message) {
        EXPECT_EQ(message.namespace_(), "zrqvn");
        EXPECT_EQ(message.source_id(), "receiver-1234");
        EXPECT_EQ(message.destination_id(), "sender-4321");
        ASSERT_EQ(message.payload_type(), CastMessage_PayloadType_STRING);
        EXPECT_EQ(message.payload_utf8(), "cnlybnq");
      }));
  task_runner_.RunTasksUntilIdle();
}

}  // namespace channel
}  // namespace cast
