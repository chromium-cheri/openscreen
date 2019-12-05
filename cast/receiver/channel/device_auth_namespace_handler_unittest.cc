// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/receiver/channel/device_auth_namespace_handler.h"

#include "cast/common/certificate/test_helpers.h"
#include "cast/common/channel/cast_socket.h"
#include "cast/common/channel/message_util.h"
#include "cast/common/channel/proto/cast_channel.pb.h"
#include "cast/common/channel/test/fake_cast_socket.h"
#include "cast/common/channel/test/mock_socket_error_handler.h"
#include "cast/common/channel/virtual_connection_manager.h"
#include "cast/common/channel/virtual_connection_router.h"
#include "cast/receiver/channel/device_auth_test_helpers.h"
#include "gtest/gtest.h"

namespace cast {
namespace channel {
namespace {

using ::testing::_;
using ::testing::Invoke;

class DeviceAuthNamespaceHandlerTest : public ::testing::Test {
 public:
  void SetUp() override {
    socket_ = fake_cast_socket_pair_.socket.get();
    router_.TakeSocket(&mock_error_handler_,
                       std::move(fake_cast_socket_pair_.socket));
    router_.AddHandlerForLocalId(kPlatformReceiverId, &auth_handler_);
  }

 protected:
  FakeCastSocketPair fake_cast_socket_pair_;
  MockSocketErrorHandler mock_error_handler_;
  CastSocket* socket_;

  StaticCredentialsProvider creds_;
  VirtualConnectionManager manager_;
  VirtualConnectionRouter router_{&manager_};
  DeviceAuthNamespaceHandler auth_handler_{&creds_};
};

}  // namespace

#define TEST_DATA_PREFIX "test/data/cast/receiver/channel/"

TEST_F(DeviceAuthNamespaceHandlerTest, AuthResponse) {
  InitStaticCredentialsFromFiles(
      &creds_, nullptr, nullptr, TEST_DATA_PREFIX "device_privkey.pem",
      TEST_DATA_PREFIX "device_chain.pem", TEST_DATA_PREFIX "device_tls.pem");

  // Send an auth challenge.  |auth_handler_| will automatically respond via
  // |router_| and we will catch the result in |challenge_reply|.
  CastMessage auth_challenge;
  std::string auth_challenge_string =
      certificate::testing::ReadEntireFileToString(TEST_DATA_PREFIX
                                                   "auth_challenge.pb");
  ASSERT_TRUE(auth_challenge.ParseFromString(auth_challenge_string));

  CastMessage challenge_reply;
  EXPECT_CALL(fake_cast_socket_pair_.mock_peer_client, OnMessage(_, _))
      .WillOnce(
          Invoke([&challenge_reply](CastSocket* socket, CastMessage message) {
            challenge_reply = std::move(message);
          }));
  fake_cast_socket_pair_.peer_socket->SendMessage(std::move(auth_challenge));

  std::string auth_response_string =
      certificate::testing::ReadEntireFileToString(TEST_DATA_PREFIX
                                                   "auth_response.pb");
  AuthResponse expected_auth_response;
  ASSERT_TRUE(expected_auth_response.ParseFromString(auth_response_string));

  DeviceAuthMessage auth_message;
  ASSERT_EQ(challenge_reply.payload_type(), CastMessage_PayloadType_BINARY);
  ASSERT_TRUE(auth_message.ParseFromString(challenge_reply.payload_binary()));
  ASSERT_TRUE(auth_message.has_response());
  ASSERT_FALSE(auth_message.has_challenge());
  ASSERT_FALSE(auth_message.has_error());
  const AuthResponse& auth_response = auth_message.response();

  EXPECT_EQ(expected_auth_response.signature(), auth_response.signature());
  EXPECT_EQ(expected_auth_response.client_auth_certificate(),
            auth_response.client_auth_certificate());
  EXPECT_EQ(expected_auth_response.signature_algorithm(),
            auth_response.signature_algorithm());
  EXPECT_EQ(expected_auth_response.sender_nonce(),
            auth_response.sender_nonce());
  EXPECT_EQ(expected_auth_response.hash_algorithm(),
            auth_response.hash_algorithm());
  EXPECT_EQ(expected_auth_response.crl(), auth_response.crl());
  ASSERT_EQ(expected_auth_response.intermediate_certificate().size(),
            auth_response.intermediate_certificate().size());
  for (int i = 0; i < auth_response.intermediate_certificate().size(); ++i) {
    EXPECT_EQ(expected_auth_response.intermediate_certificate()[i],
              auth_response.intermediate_certificate()[i])
        << i;
  }
}

}  // namespace channel
}  // namespace cast
