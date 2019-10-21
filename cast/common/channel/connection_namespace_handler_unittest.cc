// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/channel/connection_namespace_handler.h"

#include "cast/common/channel/cast_socket.h"
#include "cast/common/channel/message_util.h"
#include "cast/common/channel/virtual_connection.h"
#include "cast/common/channel/virtual_connection_manager.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/api/tls_connection.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"
#include "platform/test/mock_tls_connection.h"
#include "util/json/json_writer.h"

namespace cast {
namespace channel {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;

using openscreen::ErrorOr;
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
    ON_CALL(*connection_, Write(_, _))
        .WillByDefault(Invoke([this](const void* data, size_t len) {
          peer_connection_->OnRead(std::vector<uint8_t>(
              reinterpret_cast<const uint8_t*>(data),
              reinterpret_cast<const uint8_t*>(data) + len));
        }));
    ON_CALL(*peer_connection_, Write(_, _))
        .WillByDefault(Invoke([this](const void* data, size_t len) {
          connection_->OnRead(std::vector<uint8_t>(
              reinterpret_cast<const uint8_t*>(data),
              reinterpret_cast<const uint8_t*>(data) + len));
        }));
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

  FakeClock clock_{openscreen::platform::Clock::now()};
  FakeTaskRunner task_runner_{&clock_};
  IPEndpoint local_{{10, 0, 1, 7}, 1234};
  IPEndpoint remote_{{10, 0, 1, 9}, 4321};
  std::unique_ptr<NiceMock<MockTlsConnection>> moved_connection_{
      new NiceMock<MockTlsConnection>(&task_runner_, local_, remote_)};
  NiceMock<MockTlsConnection>* connection_{moved_connection_.get()};
  MockCastSocketClient mock_client_;
  CastSocket socket_{std::move(moved_connection_), &mock_client_, 1};

  std::unique_ptr<NiceMock<MockTlsConnection>> moved_connection2_{
      new NiceMock<MockTlsConnection>(&task_runner_, remote_, local_)};
  NiceMock<MockTlsConnection>* peer_connection_{moved_connection2_.get()};
  MockCastSocketClient mock_peer_client_;
  CastSocket peer_socket_{std::move(moved_connection2_), &mock_peer_client_, 2};

  NiceMock<MockVirtualConnectionPolicy> vc_policy_;
  VirtualConnectionManager vc_manager_;
  ConnectionNamespaceHandler connection_namespace_handler_{&vc_manager_,
                                                           &vc_policy_};

  const std::string sender_id_{"sender-5678"};
  const std::string receiver_id_{"receiver-3245"};
};

TEST_F(ConnectionNamespaceHandlerTest, Connect) {
  connection_namespace_handler_.OnMessage(
      &socket_, MakeConnectMessage(sender_id_, receiver_id_));
  EXPECT_TRUE(vc_manager_.HasConnection(
      VirtualConnection{receiver_id_, sender_id_, socket_.socket_id()}));

  EXPECT_CALL(mock_peer_client_, OnMessage(_, _)).Times(0);
  task_runner_.RunTasksUntilIdle();
}

TEST_F(ConnectionNamespaceHandlerTest, PolicyDeniesConnection) {
  EXPECT_CALL(vc_policy_, IsConnectionAllowed(_))
      .WillOnce(Invoke([](const VirtualConnection& vconn) { return false; }));
  connection_namespace_handler_.OnMessage(
      &socket_, MakeConnectMessage(sender_id_, receiver_id_));
  EXPECT_FALSE(vc_manager_.HasConnection(
      VirtualConnection{receiver_id_, sender_id_, socket_.socket_id()}));

  ExpectCloseMessage(&mock_peer_client_, receiver_id_, sender_id_);
  task_runner_.RunTasksUntilIdle();
}

TEST_F(ConnectionNamespaceHandlerTest, ConnectWithVersion) {
  connection_namespace_handler_.OnMessage(
      &socket_, MakeConnectMessage(sender_id_, receiver_id_,
                                   CastMessage_ProtocolVersion_CASTV2_1_2, {}));
  EXPECT_TRUE(vc_manager_.HasConnection(
      VirtualConnection{receiver_id_, sender_id_, socket_.socket_id()}));

  ExpectConnectedMessage(&mock_peer_client_, receiver_id_, sender_id_,
                         CastMessage_ProtocolVersion_CASTV2_1_2);
  task_runner_.RunTasksUntilIdle();
}

TEST_F(ConnectionNamespaceHandlerTest, ConnectWithVersionList) {
  connection_namespace_handler_.OnMessage(
      &socket_, MakeConnectMessage(sender_id_, receiver_id_,
                                   CastMessage_ProtocolVersion_CASTV2_1_2,
                                   {CastMessage_ProtocolVersion_CASTV2_1_3,
                                    CastMessage_ProtocolVersion_CASTV2_1_0}));
  EXPECT_TRUE(vc_manager_.HasConnection(
      VirtualConnection{receiver_id_, sender_id_, socket_.socket_id()}));

  ExpectConnectedMessage(&mock_peer_client_, receiver_id_, sender_id_,
                         CastMessage_ProtocolVersion_CASTV2_1_3);
  task_runner_.RunTasksUntilIdle();
}

TEST_F(ConnectionNamespaceHandlerTest, Close) {
  connection_namespace_handler_.OnMessage(
      &socket_, MakeConnectMessage(sender_id_, receiver_id_));
  EXPECT_TRUE(vc_manager_.HasConnection(
      VirtualConnection{receiver_id_, sender_id_, socket_.socket_id()}));
  task_runner_.RunTasksUntilIdle();

  connection_namespace_handler_.OnMessage(
      &socket_, MakeCloseMessage(sender_id_, receiver_id_));
  EXPECT_FALSE(vc_manager_.HasConnection(
      VirtualConnection{receiver_id_, sender_id_, socket_.socket_id()}));
}

TEST_F(ConnectionNamespaceHandlerTest, CloseUnknown) {
  connection_namespace_handler_.OnMessage(
      &socket_, MakeConnectMessage(sender_id_, receiver_id_));
  EXPECT_TRUE(vc_manager_.HasConnection(
      VirtualConnection{receiver_id_, sender_id_, socket_.socket_id()}));
  task_runner_.RunTasksUntilIdle();

  connection_namespace_handler_.OnMessage(
      &socket_, MakeCloseMessage(sender_id_ + "098", receiver_id_));
  EXPECT_TRUE(vc_manager_.HasConnection(
      VirtualConnection{receiver_id_, sender_id_, socket_.socket_id()}));
}

}  // namespace channel
}  // namespace cast
