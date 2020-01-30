// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/sender/cast_platform_handler.h"

#include "cast/common/channel/testing/fake_cast_socket.h"
#include "cast/common/channel/testing/mock_socket_error_handler.h"
#include "cast/common/channel/virtual_connection_manager.h"
#include "cast/common/channel/virtual_connection_router.h"
#include "cast/common/discovery/service_info.h"
#include "cast/sender/testing/test_helpers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"
#include "util/json/json_serialization.h"
#include "util/json/json_value.h"

namespace openscreen {
namespace cast {

using ::cast::channel::CastMessage;

using ::testing::_;

class CastPlatformHandlerTest : public ::testing::Test {
 public:
  void SetUp() override {
    socket_ = fake_cast_socket_pair_.socket.get();
    router_.TakeSocket(&mock_error_handler_,
                       std::move(fake_cast_socket_pair_.socket));
  }

 protected:
  CastSocket& peer_socket() { return *fake_cast_socket_pair_.peer_socket; }
  MockCastSocketClient& peer_client() {
    return fake_cast_socket_pair_.mock_peer_client;
  }

  FakeCastSocketPair fake_cast_socket_pair_;
  CastSocket* socket_;
  MockSocketErrorHandler mock_error_handler_;
  VirtualConnectionManager manager_;
  VirtualConnectionRouter router_{&manager_};
  FakeClock clock_{Clock::now()};
  FakeTaskRunner task_runner_{&clock_};
  CastPlatformHandler platform_handler_{&router_, &manager_, &FakeClock::now,
                                        &task_runner_};
};

TEST_F(CastPlatformHandlerTest, AppAvailability) {
  ServiceInfo device = {};
  device.v4_endpoint = IPEndpoint{{192, 168, 0, 17}, 4434};
  device.unique_id = "deviceId1";
  platform_handler_.OnDeviceAddedOrUpdated(device, socket_->socket_id());

  int32_t request_id = -1;
  std::string sender_id = "";
  EXPECT_CALL(peer_client(), OnMessage(_, _))
      .WillOnce([&request_id, &sender_id](CastSocket* socket,
                                          CastMessage message) {
        VerifyAppAvailabilityRequest(message, "AAA", &request_id, &sender_id);
      });
  bool ran = false;
  platform_handler_.RequestAppAvailability(
      "deviceId1", "AAA",
      [&ran](const std::string& app_id, AppAvailabilityResult availability) {
        EXPECT_EQ("AAA", app_id);
        EXPECT_EQ(availability, AppAvailabilityResult::kAvailable);
        ran = true;
      });

  CastMessage availability_response = CreateAppAvailabilityResponse(
      request_id, sender_id, "AAA", AppAvailabilityResult::kAvailable);
  EXPECT_TRUE(peer_socket().SendMessage(availability_response).ok());
  EXPECT_TRUE(ran);

  // NOTE: Callback should only fire once, so it should not fire again here.
  ran = false;
  EXPECT_TRUE(peer_socket().SendMessage(availability_response).ok());
  EXPECT_FALSE(ran);
}

}  // namespace cast
}  // namespace openscreen
