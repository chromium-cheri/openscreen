// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define SERIALIZE_THIS_TEST 0
#if SERIALIZE_THIS_TEST
#include <fcntl.h>
#include <unistd.h>
#endif

#include "cast/common/certificate/test_helpers.h"
#include "cast/common/channel/cast_socket.h"
#include "cast/common/channel/proto/cast_channel.pb.h"
#include "cast/common/channel/test/fake_cast_socket.h"
#include "cast/common/channel/test/mock_socket_error_handler.h"
#include "cast/common/channel/virtual_connection_manager.h"
#include "cast/common/channel/virtual_connection_router.h"
#include "cast/receiver/channel/device_auth_namespace_handler.h"
#include "cast/receiver/channel/device_auth_test_helpers.h"
#include "cast/sender/channel/cast_auth_util.h"
#include "cast/sender/channel/message_util.h"
#include "gtest/gtest.h"

namespace cast {
namespace channel {
namespace {

using ::testing::_;
using ::testing::Invoke;

using openscreen::ErrorOr;

class DeviceAuthTest : public ::testing::Test {
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

#define TEST_DATA_PREFIX OPENSCREEN_TEST_DATA_DIR "cast/receiver/channel/"

TEST_F(DeviceAuthTest, AuthIntegration) {
  bssl::UniquePtr<X509> parsed_cert;
  certificate::TrustStore fake_trust_store;
  InitStaticCredentialsFromFiles(&creds_, &parsed_cert, &fake_trust_store,
                                 TEST_DATA_PREFIX "device_privkey.pem",
                                 TEST_DATA_PREFIX "device_chain.pem",
                                 TEST_DATA_PREFIX "device_tls.pem");

  // Send an auth challenge.  |auth_handler_| will automatically respond via
  // |router_| and we will catch the result in |challenge_reply|.
  AuthContext auth_context = AuthContext::Create();
  CastMessage auth_challenge = CreateAuthChallengeMessage(auth_context);
#if SERIALIZE_THIS_TEST
  constexpr int kCreateFlags = O_WRONLY | O_CREAT | O_TRUNC;
  constexpr int kCreateMode = 0644;
  {
    std::string output;
    DeviceAuthMessage auth_message;
    ASSERT_EQ(auth_challenge.payload_type(), CastMessage_PayloadType_BINARY);
    ASSERT_TRUE(auth_message.ParseFromString(auth_challenge.payload_binary()));
    ASSERT_TRUE(auth_message.has_challenge());
    ASSERT_FALSE(auth_message.has_response());
    ASSERT_FALSE(auth_message.has_error());
    ASSERT_TRUE(auth_challenge.SerializeToString(&output));
    const int fd =
        open(TEST_DATA_PREFIX "auth_challenge.pb", kCreateFlags, kCreateMode);
    ASSERT_GE(fd, 0);
    ASSERT_EQ(write(fd, output.data(), output.size()), (int)output.size());
    close(fd);
  }
#endif
  CastMessage challenge_reply;
  EXPECT_CALL(fake_cast_socket_pair_.mock_peer_client, OnMessage(_, _))
      .WillOnce(
          Invoke([&challenge_reply](CastSocket* socket, CastMessage message) {
            challenge_reply = std::move(message);
          }));
  fake_cast_socket_pair_.peer_socket->SendMessage(std::move(auth_challenge));

#if SERIALIZE_THIS_TEST
  {
    std::string output;
    DeviceAuthMessage auth_message;
    ASSERT_EQ(challenge_reply.payload_type(), CastMessage_PayloadType_BINARY);
    ASSERT_TRUE(auth_message.ParseFromString(challenge_reply.payload_binary()));
    ASSERT_TRUE(auth_message.has_response());
    ASSERT_FALSE(auth_message.has_challenge());
    ASSERT_FALSE(auth_message.has_error());
    ASSERT_TRUE(auth_message.response().SerializeToString(&output));

    const int fd =
        open(TEST_DATA_PREFIX "auth_response.pb", kCreateFlags, kCreateMode);
    ASSERT_GE(fd, 0);
    ASSERT_EQ(write(fd, output.data(), output.size()), (int)output.size());
    close(fd);
  }
#endif

  certificate::DateTime October2019 = {};
  October2019.year = 2019;
  October2019.month = 10;
  October2019.day = 23;
  const ErrorOr<CastDeviceCertPolicy> error_or_policy =
      AuthenticateChallengeReplyForTest(
          challenge_reply, parsed_cert.get(), auth_context,
          certificate::CRLPolicy::kCrlOptional, &fake_trust_store, nullptr,
          October2019);
  EXPECT_TRUE(error_or_policy);
}

}  // namespace channel
}  // namespace cast
