// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/channel/connection_namespace_handler.h"

#include "cast/common/channel/cast_socket.h"
#include "cast/common/channel/message_util.h"
#include "cast/common/channel/test/fake_cast_socket.h"
#include "cast/common/channel/virtual_connection.h"
#include "cast/common/channel/virtual_connection_manager.h"
#include "cast/common/channel/virtual_connection_router.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/json/json_writer.h"

namespace cast {
namespace channel {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

using openscreen::ErrorOr;

class MockSocketErrorHandler
    : public VirtualConnectionRouter::SocketErrorHandler {
 public:
  MOCK_METHOD(void, OnClose, (CastSocket * socket), (override));
  MOCK_METHOD(void, OnError, (CastSocket * socket, Error error), (override));
};

class MockVirtualConnectionPolicy
    : public ConnectionNamespaceHandler::VirtualConnectionPolicy {
 public:
  ~MockVirtualConnectionPolicy() override = default;

  MOCK_METHOD(bool,
              IsConnectionAllowed,
              (const VirtualConnection& vconn),
              (override));
};

CastMessage MakeConnectMessage(const std::string& source_id,
                               const std::string& destination_id) {
  CastMessage connect_message;
  connect_message.set_protocol_version(CastMessage_ProtocolVersion_CASTV2_1_0);
  connect_message.set_source_id(source_id);
  connect_message.set_destination_id(destination_id);
  connect_message.set_namespace_(kConnectionNamespace);
  connect_message.set_payload_type(CastMessage_PayloadType_STRING);
  connect_message.set_payload_utf8(R"!({"type": "connect"})!");
  return connect_message;
}

CastMessage MakeConnectMessage(
    const std::string& source_id,
    const std::string& destination_id,
    absl::optional<CastMessage_ProtocolVersion> version,
    std::vector<CastMessage_ProtocolVersion> version_list) {
  CastMessage connect_message = MakeConnectMessage(source_id, destination_id);
  Json::Value message(Json::ValueType::objectValue);
  message[kKeyType] = kTypeConnect;
  if (version) {
    message[kKeyProtocolVersion] = version.value();
  }
  if (!version_list.empty()) {
    Json::Value list(Json::ValueType::arrayValue);
    for (CastMessage_ProtocolVersion v : version_list) {
      list.append(v);
    }
    message[kKeyProtocolVersionList] = std::move(list);
  }
  openscreen::JsonWriter json_writer;
  ErrorOr<std::string> result = json_writer.Write(message);
  OSP_DCHECK(result);
  connect_message.set_payload_utf8(std::move(result.value()));
  return connect_message;
}

CastMessage MakeCloseMessage(const std::string& source_id,
                             const std::string& destination_id) {
  CastMessage close_message;
  close_message.set_protocol_version(CastMessage_ProtocolVersion_CASTV2_1_0);
  close_message.set_source_id(source_id);
  close_message.set_destination_id(destination_id);
  close_message.set_namespace_(kConnectionNamespace);
  close_message.set_payload_type(CastMessage_PayloadType_STRING);
  close_message.set_payload_utf8(R"!({"type": "close"})!");
  return close_message;
}

}  // namespace

class ConnectionNamespaceHandlerTest : public ::testing::Test {
 public:
  void SetUp() override {
    socket_ = fake_cast_socket_pair_.socket.get();
    router_.TakeSocket(&mock_error_handler_,
                       std::move(fake_cast_socket_pair_.socket));

    ON_CALL(vc_policy_, IsConnectionAllowed(_))
        .WillByDefault(
            Invoke([](const VirtualConnection& vconn) { return true; }));
  }

 protected:
  void ExpectCloseMessage(MockCastSocketClient* mock_client,
                          const std::string& source_id,
                          const std::string& destination_id) {
    EXPECT_CALL(*mock_client, OnMessage(_, _))
        .WillOnce(Invoke([&source_id, &destination_id](CastSocket* socket,
                                                       CastMessage message) {
          EXPECT_EQ(message.source_id(), source_id);
          EXPECT_EQ(message.destination_id(), destination_id);
          EXPECT_EQ(message.namespace_(), kConnectionNamespace);
          ASSERT_EQ(message.payload_type(), CastMessage_PayloadType_STRING);
          openscreen::JsonReader json_reader;
          ErrorOr<Json::Value> result =
              json_reader.Read(message.payload_utf8());
          ASSERT_TRUE(result) << message.payload_utf8();
          Json::Value& value = result.value();
          const Json::Value* type =
              value.find(JSON_EXPAND_FIND_CONSTANT_ARGS(kKeyType));
          ASSERT_TRUE(type) << message.payload_utf8();
          ASSERT_TRUE(type->isString()) << message.payload_utf8();
          EXPECT_EQ(type->asString(), kTypeClose) << message.payload_utf8();
        }));
  }

  void ExpectConnectedMessage(
      MockCastSocketClient* mock_client,
      const std::string& source_id,
      const std::string& destination_id,
      absl::optional<CastMessage_ProtocolVersion> version = absl::nullopt) {
    EXPECT_CALL(*mock_client, OnMessage(_, _))
        .WillOnce(Invoke([&source_id, &destination_id, version](
                             CastSocket* socket, CastMessage message) {
          EXPECT_EQ(message.source_id(), source_id);
          EXPECT_EQ(message.destination_id(), destination_id);
          EXPECT_EQ(message.namespace_(), kConnectionNamespace);
          ASSERT_EQ(message.payload_type(), CastMessage_PayloadType_STRING);
          openscreen::JsonReader json_reader;
          ErrorOr<Json::Value> result =
              json_reader.Read(message.payload_utf8());
          ASSERT_TRUE(result) << message.payload_utf8();
          Json::Value& value = result.value();
          const Json::Value* type =
              value.find(JSON_EXPAND_FIND_CONSTANT_ARGS(kKeyType));
          ASSERT_TRUE(type) << message.payload_utf8();
          ASSERT_TRUE(type->isString()) << message.payload_utf8();
          EXPECT_EQ(type->asString(), kTypeConnected) << message.payload_utf8();
          if (version) {
            const Json::Value* message_version =
                value.find(JSON_EXPAND_FIND_CONSTANT_ARGS(kKeyProtocolVersion));
            ASSERT_TRUE(message_version) << message.payload_utf8();
            ASSERT_TRUE(message_version->isInt()) << message.payload_utf8();
            EXPECT_EQ(message_version->asInt(), version.value());
          }
        }));
  }

  FakeTaskRunner* task_runner() { return &fake_cast_socket_pair_.task_runner; }

  FakeCastSocketPair fake_cast_socket_pair_;
  MockSocketErrorHandler mock_error_handler_;
  CastSocket* socket_;

  NiceMock<MockVirtualConnectionPolicy> vc_policy_;
  VirtualConnectionManager vc_manager_;
  VirtualConnectionRouter router_{&vc_manager_};
  ConnectionNamespaceHandler connection_namespace_handler_{&vc_manager_,
                                                           &vc_policy_};

  const std::string sender_id_{"sender-5678"};
  const std::string receiver_id_{"receiver-3245"};
};

TEST_F(ConnectionNamespaceHandlerTest, Connect) {
  connection_namespace_handler_.OnMessage(
      &router_, socket_, MakeConnectMessage(sender_id_, receiver_id_));
  EXPECT_TRUE(vc_manager_.GetConnectionData(
      VirtualConnection{receiver_id_, sender_id_, socket_->socket_id()}));

  EXPECT_CALL(fake_cast_socket_pair_.mock_peer_client, OnMessage(_, _))
      .Times(0);
  task_runner()->RunTasksUntilIdle();
}

TEST_F(ConnectionNamespaceHandlerTest, PolicyDeniesConnection) {
  EXPECT_CALL(vc_policy_, IsConnectionAllowed(_))
      .WillOnce(Invoke([](const VirtualConnection& vconn) { return false; }));
  connection_namespace_handler_.OnMessage(
      &router_, socket_, MakeConnectMessage(sender_id_, receiver_id_));
  EXPECT_FALSE(vc_manager_.GetConnectionData(
      VirtualConnection{receiver_id_, sender_id_, socket_->socket_id()}));

  ExpectCloseMessage(&fake_cast_socket_pair_.mock_peer_client, receiver_id_,
                     sender_id_);
  task_runner()->RunTasksUntilIdle();
}

TEST_F(ConnectionNamespaceHandlerTest, ConnectWithVersion) {
  connection_namespace_handler_.OnMessage(
      &router_, socket_,
      MakeConnectMessage(sender_id_, receiver_id_,
                         CastMessage_ProtocolVersion_CASTV2_1_2, {}));
  EXPECT_TRUE(vc_manager_.GetConnectionData(
      VirtualConnection{receiver_id_, sender_id_, socket_->socket_id()}));

  ExpectConnectedMessage(&fake_cast_socket_pair_.mock_peer_client, receiver_id_,
                         sender_id_, CastMessage_ProtocolVersion_CASTV2_1_2);
  task_runner()->RunTasksUntilIdle();
}

TEST_F(ConnectionNamespaceHandlerTest, ConnectWithVersionList) {
  connection_namespace_handler_.OnMessage(
      &router_, socket_,
      MakeConnectMessage(sender_id_, receiver_id_,
                         CastMessage_ProtocolVersion_CASTV2_1_2,
                         {CastMessage_ProtocolVersion_CASTV2_1_3,
                          CastMessage_ProtocolVersion_CASTV2_1_0}));
  EXPECT_TRUE(vc_manager_.GetConnectionData(
      VirtualConnection{receiver_id_, sender_id_, socket_->socket_id()}));

  ExpectConnectedMessage(&fake_cast_socket_pair_.mock_peer_client, receiver_id_,
                         sender_id_, CastMessage_ProtocolVersion_CASTV2_1_3);
  task_runner()->RunTasksUntilIdle();
}

TEST_F(ConnectionNamespaceHandlerTest, Close) {
  connection_namespace_handler_.OnMessage(
      &router_, socket_, MakeConnectMessage(sender_id_, receiver_id_));
  EXPECT_TRUE(vc_manager_.GetConnectionData(
      VirtualConnection{receiver_id_, sender_id_, socket_->socket_id()}));
  task_runner()->RunTasksUntilIdle();

  connection_namespace_handler_.OnMessage(
      &router_, socket_, MakeCloseMessage(sender_id_, receiver_id_));
  EXPECT_FALSE(vc_manager_.GetConnectionData(
      VirtualConnection{receiver_id_, sender_id_, socket_->socket_id()}));
}

TEST_F(ConnectionNamespaceHandlerTest, CloseUnknown) {
  connection_namespace_handler_.OnMessage(
      &router_, socket_, MakeConnectMessage(sender_id_, receiver_id_));
  EXPECT_TRUE(vc_manager_.GetConnectionData(
      VirtualConnection{receiver_id_, sender_id_, socket_->socket_id()}));
  task_runner()->RunTasksUntilIdle();

  connection_namespace_handler_.OnMessage(
      &router_, socket_, MakeCloseMessage(sender_id_ + "098", receiver_id_));
  EXPECT_TRUE(vc_manager_.GetConnectionData(
      VirtualConnection{receiver_id_, sender_id_, socket_->socket_id()}));
}

}  // namespace channel
}  // namespace cast
