// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/sender/cast_app_discovery_service_impl.h"

#include "cast/common/channel/testing/fake_cast_socket.h"
#include "cast/common/channel/testing/mock_socket_error_handler.h"
#include "cast/common/channel/virtual_connection_manager.h"
#include "cast/common/channel/virtual_connection_router.h"
#include "cast/common/discovery/service_info.h"
#include "cast/sender/testing/test_helpers.h"
#include "gtest/gtest.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"

namespace openscreen {
namespace cast {

using ::cast::channel::CastMessage;

using ::testing::_;

class CastAppDiscoveryServiceImplTest : public ::testing::Test {
 public:
  void SetUp() override {
    socket_ = fake_cast_socket_pair_.socket.get();
    router_.TakeSocket(&mock_error_handler_,
                       std::move(fake_cast_socket_pair_.socket));

    device_.v4_endpoint = fake_cast_socket_pair_.remote;
    device_.unique_id = "deviceId1";
    device_.friendly_name = "Some Name";
  }

 protected:
  CastSocket& peer_socket() { return *fake_cast_socket_pair_.peer_socket; }
  MockCastSocketClient& peer_client() {
    return fake_cast_socket_pair_.mock_peer_client;
  }

  std::unique_ptr<CastAppDiscoveryService::Subscription> StartObservingDevices(
      const CastMediaSource& source,
      std::vector<ServiceInfo>* save_devices) {
    return app_discovery_service_.StartObservingDevices(
        source, [save_devices](const CastMediaSource& source,
                               const std::vector<ServiceInfo>& devices) {
          *save_devices = devices;
        });
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
  CastAppDiscoveryServiceImpl app_discovery_service_{&platform_handler_,
                                                     &FakeClock::now};

  CastMediaSource source_a_1_{"cast:AAA?clientId=1", {"AAA"}};
  CastMediaSource source_a_2_{"cast:AAA?clientId=2", {"AAA"}};
  CastMediaSource source_b_1_{"cast:BBB?clientId=1", {"BBB"}};

  ServiceInfo device_;
};

TEST_F(CastAppDiscoveryServiceImplTest, StartObservingDevices) {
  std::vector<ServiceInfo> devices1;
  auto subscription1 = StartObservingDevices(source_a_1_, &devices1);

  // Adding a device after app registered causes app availability request to be
  // sent.
  int32_t request_id = -1;
  std::string sender_id = "";
  EXPECT_CALL(peer_client(), OnMessage(_, _))
      .WillOnce([&request_id, &sender_id](CastSocket*, CastMessage message) {
        VerifyAppAvailabilityRequest(message, "AAA", &request_id, &sender_id);
      });

  platform_handler_.OnDeviceAddedOrUpdated(device_, socket_->socket_id());
  app_discovery_service_.OnDeviceAddedOrUpdated(device_);

  // Same app ID should not trigger another request.
  EXPECT_CALL(peer_client(), OnMessage(_, _)).Times(0);
  std::vector<ServiceInfo> devices2;
  auto subscription2 = StartObservingDevices(source_a_2_, &devices2);

  CastMessage availability_response = CreateAppAvailabilityResponse(
      request_id, sender_id, "AAA", AppAvailabilityResult::kAvailable);
  EXPECT_TRUE(peer_socket().SendMessage(availability_response).ok());
  ASSERT_EQ(devices1.size(), 1u);
  ASSERT_EQ(devices2.size(), 1u);
  EXPECT_EQ(devices1[0].unique_id, "deviceId1");
  EXPECT_EQ(devices2[0].unique_id, "deviceId1");

  // No more updates for |source_a_1_| (i.e. |devices1|).
  subscription1.reset();
  platform_handler_.OnDeviceRemoved(device_);
  app_discovery_service_.OnDeviceRemoved(device_);
  ASSERT_EQ(devices1.size(), 1u);
  EXPECT_EQ(devices2.size(), 0u);
  EXPECT_EQ(devices1[0].unique_id, "deviceId1");
}

TEST_F(CastAppDiscoveryServiceImplTest, ReAddDeviceQueryUsesCachedValue) {
  std::vector<ServiceInfo> devices1;
  auto subscription1 = StartObservingDevices(source_a_1_, &devices1);

  // Adding a device after app registered causes app availability request to be
  // sent.
  int32_t request_id = -1;
  std::string sender_id = "";
  EXPECT_CALL(peer_client(), OnMessage(_, _))
      .WillOnce([&request_id, &sender_id](CastSocket*, CastMessage message) {
        VerifyAppAvailabilityRequest(message, "AAA", &request_id, &sender_id);
      });

  platform_handler_.OnDeviceAddedOrUpdated(device_, socket_->socket_id());
  app_discovery_service_.OnDeviceAddedOrUpdated(device_);

  CastMessage availability_response = CreateAppAvailabilityResponse(
      request_id, sender_id, "AAA", AppAvailabilityResult::kAvailable);
  EXPECT_TRUE(peer_socket().SendMessage(availability_response).ok());
  ASSERT_EQ(devices1.size(), 1u);
  EXPECT_EQ(devices1[0].unique_id, "deviceId1");

  subscription1.reset();
  devices1.clear();

  // Request not re-sent; cached kAvailable value is used.
  EXPECT_CALL(peer_client(), OnMessage(_, _)).Times(0);
  subscription1 = StartObservingDevices(source_a_1_, &devices1);
  ASSERT_EQ(devices1.size(), 1u);
  EXPECT_EQ(devices1[0].unique_id, "deviceId1");
}

TEST_F(CastAppDiscoveryServiceImplTest, DeviceQueryUpdatedOnDeviceUpdate) {
  std::vector<ServiceInfo> devices1;
  auto subscription1 = StartObservingDevices(source_a_1_, &devices1);

  // Adding a device after app registered causes app availability request to be
  // sent.
  int32_t request_id = -1;
  std::string sender_id = "";
  EXPECT_CALL(peer_client(), OnMessage(_, _))
      .WillOnce([&request_id, &sender_id](CastSocket*, CastMessage message) {
        VerifyAppAvailabilityRequest(message, "AAA", &request_id, &sender_id);
      });

  platform_handler_.OnDeviceAddedOrUpdated(device_, socket_->socket_id());
  app_discovery_service_.OnDeviceAddedOrUpdated(device_);

  // Result set now includes |device_|.
  CastMessage availability_response = CreateAppAvailabilityResponse(
      request_id, sender_id, "AAA", AppAvailabilityResult::kAvailable);
  EXPECT_TRUE(peer_socket().SendMessage(availability_response).ok());
  ASSERT_EQ(devices1.size(), 1u);
  EXPECT_EQ(devices1[0].unique_id, "deviceId1");

  // Updating |device_| causes |source_a_1_| query to be updated, but it's too
  // soon for a new message to be sent.
  EXPECT_CALL(peer_client(), OnMessage(_, _)).Times(0);
  device_.friendly_name = "New Name";

  platform_handler_.OnDeviceAddedOrUpdated(device_, socket_->socket_id());
  app_discovery_service_.OnDeviceAddedOrUpdated(device_);

  ASSERT_EQ(devices1.size(), 1u);
  EXPECT_EQ(devices1[0].friendly_name, "New Name");
}

TEST_F(CastAppDiscoveryServiceImplTest, Refresh) {
  std::vector<ServiceInfo> devices1;
  auto subscription1 = StartObservingDevices(source_a_1_, &devices1);
  std::vector<ServiceInfo> devices2;
  auto subscription2 = StartObservingDevices(source_b_1_, &devices2);

  // Adding a device after app registered causes two separate app availability
  // requests to be sent.
  int32_t request_idA = -1;
  int32_t request_idB = -1;
  std::string sender_id = "";
  EXPECT_CALL(peer_client(), OnMessage(_, _))
      .Times(2)
      .WillRepeatedly([&request_idA, &request_idB, &sender_id](
                          CastSocket*, CastMessage message) {
        std::string app_id;
        int32_t request_id = -1;
        VerifyAppAvailabilityRequest(message, &app_id, &request_id, &sender_id);
        if (app_id == "AAA") {
          EXPECT_EQ(request_idA, -1);
          request_idA = request_id;
        } else if (app_id == "BBB") {
          EXPECT_EQ(request_idB, -1);
          request_idB = request_id;
        } else {
          EXPECT_TRUE(false);
        }
      });

  platform_handler_.OnDeviceAddedOrUpdated(device_, socket_->socket_id());
  app_discovery_service_.OnDeviceAddedOrUpdated(device_);

  CastMessage availability_response = CreateAppAvailabilityResponse(
      request_idA, sender_id, "AAA", AppAvailabilityResult::kAvailable);
  EXPECT_TRUE(peer_socket().SendMessage(availability_response).ok());
  availability_response = CreateAppAvailabilityResponse(
      request_idB, sender_id, "BBB", AppAvailabilityResult::kUnavailable);
  EXPECT_TRUE(peer_socket().SendMessage(availability_response).ok());
  ASSERT_EQ(devices1.size(), 1u);
  ASSERT_EQ(devices2.size(), 0u);
  EXPECT_EQ(devices1[0].unique_id, "deviceId1");

  // Not enough time has passed for a refresh.
  clock_.Advance(std::chrono::seconds(30));
  EXPECT_CALL(peer_client(), OnMessage(_, _)).Times(0);
  app_discovery_service_.Refresh();

  // Refresh will now query again for unavailable app IDs.
  clock_.Advance(std::chrono::minutes(2));
  EXPECT_CALL(peer_client(), OnMessage(_, _))
      .WillOnce([&request_idB, &sender_id](CastSocket*, CastMessage message) {
        VerifyAppAvailabilityRequest(message, "BBB", &request_idB, &sender_id);
      });
  app_discovery_service_.Refresh();
}

TEST_F(CastAppDiscoveryServiceImplTest, StartObservingDevicesAfterDeviceAdded) {
  // No registered apps.
  EXPECT_CALL(peer_client(), OnMessage(_, _)).Times(0);
  platform_handler_.OnDeviceAddedOrUpdated(device_, socket_->socket_id());
  app_discovery_service_.OnDeviceAddedOrUpdated(device_);

  // Registering apps immediately sends requests to |device_|.
  int32_t request_idA = -1;
  std::string sender_id = "";
  EXPECT_CALL(peer_client(), OnMessage(_, _))
      .WillOnce([&request_idA, &sender_id](CastSocket*, CastMessage message) {
        VerifyAppAvailabilityRequest(message, "AAA", &request_idA, &sender_id);
      });
  std::vector<ServiceInfo> devices1;
  auto subscription1 = StartObservingDevices(source_a_1_, &devices1);

  int32_t request_idB = -1;
  EXPECT_CALL(peer_client(), OnMessage(_, _))
      .WillOnce([&request_idB, &sender_id](CastSocket*, CastMessage message) {
        VerifyAppAvailabilityRequest(message, "BBB", &request_idB, &sender_id);
      });
  std::vector<ServiceInfo> devices2;
  auto subscription2 = StartObservingDevices(source_b_1_, &devices2);

  // Add a new device with a corresponding socket.
  FakeCastSocketPair fake_sockets2({{192, 168, 1, 17}, 2345},
                                   {{192, 168, 1, 19}, 2345}, 3, 4);
  CastSocket* socket2 = fake_sockets2.socket.get();
  router_.TakeSocket(&mock_error_handler_, std::move(fake_sockets2.socket));
  ServiceInfo device2;
  device2.unique_id = "deviceId2";
  device2.v4_endpoint = fake_sockets2.remote;

  // Adding new device causes availability requests for both apps to be sent to
  // the new device.
  request_idA = -1;
  request_idB = -1;
  EXPECT_CALL(fake_sockets2.mock_peer_client, OnMessage(_, _))
      .Times(2)
      .WillRepeatedly([&request_idA, &request_idB, &sender_id](
                          CastSocket*, CastMessage message) {
        std::string app_id;
        int32_t request_id = -1;
        VerifyAppAvailabilityRequest(message, &app_id, &request_id, &sender_id);
        if (app_id == "AAA") {
          EXPECT_EQ(request_idA, -1);
          request_idA = request_id;
        } else if (app_id == "BBB") {
          EXPECT_EQ(request_idB, -1);
          request_idB = request_id;
        } else {
          EXPECT_TRUE(false);
        }
      });

  platform_handler_.OnDeviceAddedOrUpdated(device2, socket2->socket_id());
  app_discovery_service_.OnDeviceAddedOrUpdated(device2);
}

TEST_F(CastAppDiscoveryServiceImplTest, StartObservingDevicesCachedValue) {
  std::vector<ServiceInfo> devices1;
  auto subscription1 = StartObservingDevices(source_a_1_, &devices1);

  // Adding a device after app registered causes app availability request to be
  // sent.
  int32_t request_id = -1;
  std::string sender_id = "";
  EXPECT_CALL(peer_client(), OnMessage(_, _))
      .WillOnce([&request_id, &sender_id](CastSocket*, CastMessage message) {
        VerifyAppAvailabilityRequest(message, "AAA", &request_id, &sender_id);
      });

  platform_handler_.OnDeviceAddedOrUpdated(device_, socket_->socket_id());
  app_discovery_service_.OnDeviceAddedOrUpdated(device_);

  CastMessage availability_response = CreateAppAvailabilityResponse(
      request_id, sender_id, "AAA", AppAvailabilityResult::kAvailable);
  EXPECT_TRUE(peer_socket().SendMessage(availability_response).ok());
  ASSERT_EQ(devices1.size(), 1u);
  EXPECT_EQ(devices1[0].unique_id, "deviceId1");

  // Same app ID should not trigger another request, but it should return
  // cached value.
  EXPECT_CALL(peer_client(), OnMessage(_, _)).Times(0);
  std::vector<ServiceInfo> devices2;
  auto subscription2 = StartObservingDevices(source_a_2_, &devices2);
  ASSERT_EQ(devices2.size(), 1u);
  EXPECT_EQ(devices2[0].unique_id, "deviceId1");
}

TEST_F(CastAppDiscoveryServiceImplTest, AvailabilityUnknownOrUnavailable) {
  std::vector<ServiceInfo> devices1;
  auto subscription1 = StartObservingDevices(source_a_1_, &devices1);

  // Adding a device after app registered causes app availability request to be
  // sent.
  int32_t request_id = -1;
  std::string sender_id = "";
  EXPECT_CALL(peer_client(), OnMessage(_, _))
      .WillOnce([&request_id, &sender_id](CastSocket*, CastMessage message) {
        VerifyAppAvailabilityRequest(message, "AAA", &request_id, &sender_id);
      });

  platform_handler_.OnDeviceAddedOrUpdated(device_, socket_->socket_id());
  app_discovery_service_.OnDeviceAddedOrUpdated(device_);

  // The request will timeout resulting in unknown app availability.
  clock_.Advance(std::chrono::seconds(10));
  task_runner_.RunTasksUntilIdle();
  EXPECT_EQ(devices1.size(), 0u);

  // Device updated together with unknown app availability will cause a request
  // to be sent again.
  request_id = -1;
  EXPECT_CALL(peer_client(), OnMessage(_, _))
      .WillOnce([&request_id, &sender_id](CastSocket*, CastMessage message) {
        VerifyAppAvailabilityRequest(message, "AAA", &request_id, &sender_id);
      });
  platform_handler_.OnDeviceAddedOrUpdated(device_, socket_->socket_id());
  app_discovery_service_.OnDeviceAddedOrUpdated(device_);

  CastMessage availability_response = CreateAppAvailabilityResponse(
      request_id, sender_id, "AAA", AppAvailabilityResult::kUnavailable);
  EXPECT_TRUE(peer_socket().SendMessage(availability_response).ok());

  // Known availability so no request sent.
  EXPECT_CALL(peer_client(), OnMessage(_, _)).Times(0);
  platform_handler_.OnDeviceAddedOrUpdated(device_, socket_->socket_id());
  app_discovery_service_.OnDeviceAddedOrUpdated(device_);

  // Removing the device will also remove previous availability information.
  // Next time the device is added, a new request will be sent.
  platform_handler_.OnDeviceRemoved(device_);
  app_discovery_service_.OnDeviceRemoved(device_);

  request_id = -1;
  EXPECT_CALL(peer_client(), OnMessage(_, _))
      .WillOnce([&request_id, &sender_id](CastSocket*, CastMessage message) {
        VerifyAppAvailabilityRequest(message, "AAA", &request_id, &sender_id);
      });

  platform_handler_.OnDeviceAddedOrUpdated(device_, socket_->socket_id());
  app_discovery_service_.OnDeviceAddedOrUpdated(device_);
}

}  // namespace cast
}  // namespace openscreen
