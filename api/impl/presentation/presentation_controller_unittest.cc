// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/presentation/presentation_controller.h"

#include <string>
#include <vector>

#include "api/impl/quic/testing/quic_test_support.h"
#include "api/impl/service_listener_impl.h"
#include "api/impl/testing/fake_clock.h"
#include "api/public/message_demuxer.h"
#include "api/public/network_service_manager.h"
<<<<<<< HEAD
#include "api/public/testing/message_demuxer_test_support.h"
=======
>>>>>>> Add Controller implementation for URL availability
#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {
namespace presentation {

<<<<<<< HEAD
using std::chrono::seconds;
using ::testing::_;
using ::testing::Invoke;

constexpr char kTestUrl[] = "https://example.foo";
=======
using ::testing::_;
using ::testing::Invoke;

const char kTestUrl[] = "https://example.foo";

// TODO(btolsch): Put this in a separate file since multiple places use it now.
class MockMessageCallback final : public MessageDemuxer::MessageCallback {
 public:
  ~MockMessageCallback() override = default;

  MOCK_METHOD6(OnStreamMessage,
               ErrorOr<size_t>(uint64_t endpoint_id,
                               uint64_t connection_id,
                               msgs::Type message_type,
                               const uint8_t* buffer,
                               size_t buffer_size,
                               platform::TimeDelta now));
};
>>>>>>> Add Controller implementation for URL availability

class MockServiceListenerDelegate final : public ServiceListenerImpl::Delegate {
 public:
  ~MockServiceListenerDelegate() override = default;

  ServiceListenerImpl* listener() { return listener_; }

  MOCK_METHOD0(StartListener, void());
  MOCK_METHOD0(StartAndSuspendListener, void());
  MOCK_METHOD0(StopListener, void());
  MOCK_METHOD0(SuspendListener, void());
  MOCK_METHOD0(ResumeListener, void());
  MOCK_METHOD1(SearchNow, void(ServiceListener::State from));
};

class MockReceiverObserver final : public ReceiverObserver {
 public:
  ~MockReceiverObserver() override = default;

  MOCK_METHOD2(OnRequestFailed,
               void(const std::string& presentation_url,
                    const std::string& service_id));
  MOCK_METHOD2(OnReceiverAvailable,
               void(const std::string& presentation_url,
                    const std::string& service_id));
  MOCK_METHOD2(OnReceiverUnavailable,
               void(const std::string& presentation_url,
                    const std::string& service_id));
};

// TODO(btolsch): This is also used in multiple places now; factor out to
// separate file.
class MockConnectionDelegate final : public Connection::Delegate {
 public:
  MockConnectionDelegate() = default;
  ~MockConnectionDelegate() override = default;

  MOCK_METHOD0(OnConnected, void());
  MOCK_METHOD0(OnClosedByRemote, void());
  MOCK_METHOD0(OnDiscarded, void());
  MOCK_METHOD1(OnError, void(const absl::string_view message));
  MOCK_METHOD0(OnTerminated, void());
  MOCK_METHOD1(OnStringMessage, void(const absl::string_view message));
  MOCK_METHOD1(OnBinaryMessage, void(const std::vector<uint8_t>& data));
};

class MockRequestDelegate final : public RequestDelegate {
 public:
  MockRequestDelegate() = default;
  ~MockRequestDelegate() override = default;

  void OnConnection(std::unique_ptr<Connection> connection) override {
    OnConnectionMock(connection);
  }
  MOCK_METHOD1(OnConnectionMock, void(std::unique_ptr<Connection>& connection));
  MOCK_METHOD1(OnError, void(const Error& error));
};

class ControllerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto service_listener =
        std::make_unique<ServiceListenerImpl>(&mock_listener_delegate_);
    NetworkServiceManager::Create(std::move(service_listener), nullptr,
                                  std::move(quic_bridge_.quic_client),
                                  std::move(quic_bridge_.quic_server));
<<<<<<< HEAD
    controller_ = std::make_unique<Controller>(FakeClock::now);
=======
    controller_ = std::make_unique<Controller>(
        std::make_unique<FakeClock>(platform::TimeDelta::FromSeconds(11111)));
>>>>>>> Add Controller implementation for URL availability
    ON_CALL(quic_bridge_.mock_server_observer, OnIncomingConnectionMock(_))
        .WillByDefault(
            Invoke([this](std::unique_ptr<ProtocolConnection>& connection) {
              controller_endpoint_id_ = connection->endpoint_id();
            }));
<<<<<<< HEAD

    availability_watch_ =
        quic_bridge_.receiver_demuxer->SetDefaultMessageTypeWatch(
            msgs::Type::kPresentationUrlAvailabilityRequest, &mock_callback_);
  }

  void TearDown() override {
    availability_watch_ = MessageDemuxer::MessageWatch();
=======
  }

  void TearDown() override {
>>>>>>> Add Controller implementation for URL availability
    controller_.reset();
    NetworkServiceManager::Dispose();
  }

<<<<<<< HEAD
  void ExpectAvailabilityRequest(
      MockMessageCallback* mock_callback,
      msgs::PresentationUrlAvailabilityRequest* request) {
    EXPECT_CALL(*mock_callback, OnStreamMessage(_, _, _, _, _, _))
        .WillOnce(Invoke([request](uint64_t endpoint_id, uint64_t cid,
                                   msgs::Type message_type,
                                   const uint8_t* buffer, size_t buffer_size,
                                   platform::Clock::time_point now) {
          ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
              buffer, buffer_size, request);
          return result;
        }));
  }

=======
>>>>>>> Add Controller implementation for URL availability
  void SendAvailabilityResponse(
      const msgs::PresentationUrlAvailabilityResponse& response) {
    std::unique_ptr<ProtocolConnection> controller_connection =
        NetworkServiceManager::Get()
            ->GetProtocolConnectionServer()
            ->CreateProtocolConnection(controller_endpoint_id_);
    ASSERT_TRUE(controller_connection);
    ASSERT_EQ(Error::Code::kNone,
              controller_connection
                  ->WriteMessage(
                      response, msgs::EncodePresentationUrlAvailabilityResponse)
                  .code());
  }

<<<<<<< HEAD
<<<<<<< HEAD
  void SendAvailabilityEvent(
      const msgs::PresentationUrlAvailabilityEvent& event) {
=======
  void SendInitiationResponse(
      const msgs::PresentationInitiationResponse& response) {
>>>>>>> Add start/terminate presentation support
    std::unique_ptr<ProtocolConnection> controller_connection =
        NetworkServiceManager::Get()
            ->GetProtocolConnectionServer()
            ->CreateProtocolConnection(controller_endpoint_id_);
    ASSERT_TRUE(controller_connection);
    ASSERT_EQ(
        Error::Code::kNone,
        controller_connection
<<<<<<< HEAD
            ->WriteMessage(event, msgs::EncodePresentationUrlAvailabilityEvent)
            .code());
  }

  MessageDemuxer::MessageWatch availability_watch_;
  MockMessageCallback mock_callback_;
  FakeClock fake_clock_{platform::Clock::time_point(seconds(11111))};
  FakeQuicBridge quic_bridge_{FakeClock::now};
  MockServiceListenerDelegate mock_listener_delegate_;
=======
=======
            ->WriteMessage(response, msgs::EncodePresentationInitiationResponse)
            .code());
  }

>>>>>>> Add start/terminate presentation support
  MockServiceListenerDelegate mock_listener_delegate_;
  FakeQuicBridge quic_bridge_;
>>>>>>> Add Controller implementation for URL availability
  std::unique_ptr<Controller> controller_;
  ServiceInfo receiver_info1{"service-id1",
                             "lucas-auer",
                             1,
                             quic_bridge_.kReceiverEndpoint,
                             {}};
  MockReceiverObserver mock_receiver_observer_;
  uint64_t controller_endpoint_id_{0};
};

TEST_F(ControllerTest, ReceiverWatchMoves) {
  std::vector<std::string> urls{"one fish", "two fish", "red fish", "gnu fish"};
  MockReceiverObserver mock_observer;

<<<<<<< HEAD
  Controller::ReceiverWatch watch1(controller_.get(), urls, &mock_observer);
=======
  Controller::ReceiverWatch watch1(urls, &mock_observer, controller_.get());
>>>>>>> Add Controller implementation for URL availability
  EXPECT_TRUE(watch1);
  Controller::ReceiverWatch watch2;
  EXPECT_FALSE(watch2);
  watch2 = std::move(watch1);
  EXPECT_FALSE(watch1);
  EXPECT_TRUE(watch2);
  Controller::ReceiverWatch watch3(std::move(watch2));
  EXPECT_FALSE(watch2);
  EXPECT_TRUE(watch3);
}

TEST_F(ControllerTest, ConnectRequestMoves) {
  std::string service_id{"service-id1"};
  uint64_t request_id = 7;

<<<<<<< HEAD
  Controller::ConnectRequest request1(controller_.get(), service_id, false,
                                      request_id);
=======
  Controller::ConnectRequest request1(service_id, false, request_id,
                                      controller_.get());
>>>>>>> Add Controller implementation for URL availability
  EXPECT_TRUE(request1);
  Controller::ConnectRequest request2;
  EXPECT_FALSE(request2);
  request2 = std::move(request1);
  EXPECT_FALSE(request1);
  EXPECT_TRUE(request2);
  Controller::ConnectRequest request3(std::move(request2));
  EXPECT_FALSE(request2);
  EXPECT_TRUE(request3);
}

<<<<<<< HEAD
TEST_F(ControllerTest, ReceiverAvailable) {
=======
TEST_F(ControllerTest, ReceiverAvailableFirstWatch) {
  MockMessageCallback mock_callback;
  MessageDemuxer::MessageWatch availability_watch =
      quic_bridge_.receiver_demuxer->SetDefaultMessageTypeWatch(
          msgs::Type::kPresentationUrlAvailabilityRequest, &mock_callback);

>>>>>>> Add Controller implementation for URL availability
  mock_listener_delegate_.listener()->OnReceiverAdded(receiver_info1);
  Controller::ReceiverWatch watch =
      controller_->RegisterReceiverWatch({kTestUrl}, &mock_receiver_observer_);

  msgs::PresentationUrlAvailabilityRequest request;
<<<<<<< HEAD
  ExpectAvailabilityRequest(&mock_callback_, &request);
=======
  EXPECT_CALL(mock_callback, OnStreamMessage(_, _, _, _, _, _))
      .WillOnce(
          Invoke([&request](uint64_t endpoint_id, uint64_t cid,
                            msgs::Type message_type, const uint8_t* buffer,
                            size_t buffer_size, platform::TimeDelta now) {
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            return result;
          }));
>>>>>>> Add Controller implementation for URL availability
  quic_bridge_.RunTasksUntilIdle();

  msgs::PresentationUrlAvailabilityResponse response;
  response.request_id = request.request_id;
<<<<<<< HEAD
  response.url_availabilities.push_back(
      msgs::PresentationUrlAvailability::kCompatible);
  SendAvailabilityResponse(response);
  EXPECT_CALL(mock_receiver_observer_, OnReceiverAvailable(_, _));
  quic_bridge_.RunTasksUntilIdle();

  MockReceiverObserver mock_receiver_observer2;
  EXPECT_CALL(mock_receiver_observer2, OnReceiverAvailable(_, _));
  Controller::ReceiverWatch watch2 =
      controller_->RegisterReceiverWatch({kTestUrl}, &mock_receiver_observer2);
}

TEST_F(ControllerTest, ReceiverWatchCancel) {
=======
  response.url_availabilities.push_back(msgs::kCompatible);
  SendAvailabilityResponse(response);
  EXPECT_CALL(mock_receiver_observer_, OnReceiverAvailable(_, _));
  quic_bridge_.RunTasksUntilIdle();
}

TEST_F(ControllerTest, ReceiverAlreadyAvailableBeforeWatch) {
  MockMessageCallback mock_callback;
  MessageDemuxer::MessageWatch availability_watch =
      quic_bridge_.receiver_demuxer->SetDefaultMessageTypeWatch(
          msgs::Type::kPresentationUrlAvailabilityRequest, &mock_callback);

>>>>>>> Add Controller implementation for URL availability
  mock_listener_delegate_.listener()->OnReceiverAdded(receiver_info1);
  Controller::ReceiverWatch watch =
      controller_->RegisterReceiverWatch({kTestUrl}, &mock_receiver_observer_);

  msgs::PresentationUrlAvailabilityRequest request;
<<<<<<< HEAD
  ExpectAvailabilityRequest(&mock_callback_, &request);
=======
  EXPECT_CALL(mock_callback, OnStreamMessage(_, _, _, _, _, _))
      .WillOnce(
          Invoke([&request](uint64_t endpoint_id, uint64_t cid,
                            msgs::Type message_type, const uint8_t* buffer,
                            size_t buffer_size, platform::TimeDelta now) {
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            return result;
          }));
>>>>>>> Add Controller implementation for URL availability
  quic_bridge_.RunTasksUntilIdle();

  msgs::PresentationUrlAvailabilityResponse response;
  response.request_id = request.request_id;
<<<<<<< HEAD
  response.url_availabilities.push_back(
      msgs::PresentationUrlAvailability::kCompatible);
  SendAvailabilityResponse(response);
  EXPECT_CALL(mock_receiver_observer_, OnReceiverAvailable(_, _));
=======
  response.url_availabilities.push_back(msgs::kCompatible);
  SendAvailabilityResponse(response);
>>>>>>> Add Controller implementation for URL availability
  quic_bridge_.RunTasksUntilIdle();

  MockReceiverObserver mock_receiver_observer2;
  EXPECT_CALL(mock_receiver_observer2, OnReceiverAvailable(_, _));
  Controller::ReceiverWatch watch2 =
      controller_->RegisterReceiverWatch({kTestUrl}, &mock_receiver_observer2);
<<<<<<< HEAD

  watch = Controller::ReceiverWatch();
  msgs::PresentationUrlAvailabilityEvent event;
  event.watch_id = request.watch_id;
  event.urls.emplace_back(kTestUrl);
  event.url_availabilities.push_back(
      msgs::PresentationUrlAvailability::kNotCompatible);

  EXPECT_CALL(mock_receiver_observer2, OnReceiverUnavailable(_, _));
  EXPECT_CALL(mock_receiver_observer_, OnReceiverUnavailable(_, _)).Times(0);
  SendAvailabilityEvent(event);
  quic_bridge_.RunTasksUntilIdle();
=======
>>>>>>> Add Controller implementation for URL availability
}

TEST_F(ControllerTest, StartPresentation) {
  MockMessageCallback mock_callback;
  MessageDemuxer::MessageWatch start_presentation_watch =
      quic_bridge_.receiver_demuxer->SetDefaultMessageTypeWatch(
          msgs::Type::kPresentationInitiationRequest, &mock_callback);
  mock_listener_delegate_.listener()->OnReceiverAdded(receiver_info1);
  quic_bridge_.RunTasksUntilIdle();

  MockRequestDelegate mock_request_delegate;
  MockConnectionDelegate mock_connection_delegate;
  msgs::PresentationInitiationRequest request;
  EXPECT_CALL(mock_callback, OnStreamMessage(_, _, _, _, _, _))
      .WillOnce(
          Invoke([&request](uint64_t endpoint_id, uint64_t cid,
                            msgs::Type message_type, const uint8_t* buffer,
                            size_t buffer_size, platform::TimeDelta now) {
            ssize_t result = msgs::DecodePresentationInitiationRequest(
                buffer, buffer_size, &request);
            return result;
          }));
  Controller::ConnectRequest connect_request = controller_->StartPresentation(
      "https://example.com/receiver.html", receiver_info1.service_id,
      &mock_request_delegate, &mock_connection_delegate);
  ASSERT_TRUE(connect_request);
  quic_bridge_.RunTasksUntilIdle();

  msgs::PresentationInitiationResponse response;
  response.request_id = request.request_id;
  response.result = static_cast<decltype(response.result)>(msgs::kSuccess);
  response.has_connection_result = true;
  response.connection_result =
      static_cast<decltype(response.connection_result)>(msgs::kSuccess);
  SendInitiationResponse(response);

  EXPECT_CALL(mock_request_delegate, OnConnectionMock(_));
  EXPECT_CALL(mock_connection_delegate, OnConnected());
  quic_bridge_.RunTasksUntilIdle();
}

}  // namespace presentation
}  // namespace openscreen
