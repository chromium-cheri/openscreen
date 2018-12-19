// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/presentation/url_availability_listener.h"

#include "api/impl/quic/testing/fake_quic_connection_factory.h"
#include "api/impl/testing/fake_clock.h"
#include "base/make_unique.h"
#include "msgs/osp_messages.h"
#include "platform/api/logging.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "api/impl/quic/quic_client.h"
#include "api/public/network_service_manager.h"

namespace openscreen {
namespace presentation {

using ::testing::_;

class MockScreenObserver final : public ScreenObserver {
 public:
  ~MockScreenObserver() override = default;

  MOCK_METHOD2(OnScreensAvailable,
               void(const std::string&, const std::string&));
  MOCK_METHOD2(OnScreensUnavailable,
               void(const std::string&, const std::string&));
};

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

class NullObserver final : public ProtocolConnectionServiceObserver {
 public:
  ~NullObserver() override = default;
  void OnRunning() override {}
  void OnStopped() override {}
  void OnMetrics(const NetworkMetrics& metrics) override {}
  void OnError(const Error& error) override {}
};

class UrlAvailabilityListenerTest : public ::testing::Test {
 public:
  void SetUp() override {
    platform::TimeDelta now = platform::TimeDelta::FromMilliseconds(1298424);
    auto fake_clock = MakeUnique<FakeClock>(now);
    controller_fake_clock_ = fake_clock.get();
    controller_demuxer_ = MakeUnique<MessageDemuxer>(
        MessageDemuxer::kDefaultBufferLimit, std::move(fake_clock));

    fake_clock = MakeUnique<FakeClock>(now);
    receiver_fake_clock_ = fake_clock.get();
    receiver_demuxer_ = MakeUnique<MessageDemuxer>(
        MessageDemuxer::kDefaultBufferLimit, std::move(fake_clock));

    auto fake_factory = MakeUnique<FakeQuicConnectionFactory>(
        controller_endpoint_, receiver_demuxer_.get());
    fake_factory_ = fake_factory.get();
    auto quic_client = MakeUnique<QuicClient>(
        controller_demuxer_.get(), std::move(fake_factory), &null_observer_);
    quic_client->Start();
    NetworkServiceManager::Get()->Create(nullptr, nullptr,
                                         std::move(quic_client), nullptr);
  }

  void TearDown() override { NetworkServiceManager::Get()->Dispose(); }

 protected:
  void RunTasksUntilIdle() {
    do {
      NetworkServiceManager::Get()->GetProtocolConnectionClient()->RunTasks();
    } while (!fake_factory_->idle());
  }

  void SendAvailabilityResponse(
      const msgs::PresentationUrlAvailabilityRequest& request,
      std::vector<msgs::PresentationUrlAvailability>&& availabilities,
      FakeQuicStream* stream) {
    msgs::PresentationUrlAvailabilityResponse response;
    response.request_id = request.request_id;
    response.url_availabilities = std::move(availabilities);
    msgs::CborEncodeBuffer buffer;
    ssize_t encode_result =
        msgs::EncodePresentationUrlAvailabilityResponse(response, &buffer);
    ASSERT_GT(encode_result, 0);
    stream->ReceiveData(buffer.data(), buffer.size());
  }

  void SendAvailabilityEvent(
      uint64_t watch_id,
      std::vector<std::string>&& urls,
      std::vector<msgs::PresentationUrlAvailability>&& availabilities,
      FakeQuicStream* stream) {
    msgs::PresentationUrlAvailabilityEvent event;
    event.watch_id = watch_id;
    event.urls = std::move(urls);
    event.url_availabilities = std::move(availabilities);
    msgs::CborEncodeBuffer buffer;
    ssize_t encode_result =
        msgs::EncodePresentationUrlAvailabilityEvent(event, &buffer);
    ASSERT_GT(encode_result, 0);
    stream->ReceiveData(buffer.data(), buffer.size());
  }

  FakeClock* controller_fake_clock_;
  FakeClock* receiver_fake_clock_;
  const IPEndpoint controller_endpoint_{{192, 168, 1, 3}, 4321};
  const IPEndpoint receiver_endpoint_{{192, 168, 1, 17}, 1234};
  std::unique_ptr<MessageDemuxer> controller_demuxer_;
  std::unique_ptr<MessageDemuxer> receiver_demuxer_;
  FakeQuicConnectionFactory* fake_factory_;
  NullObserver null_observer_;
};

TEST_F(UrlAvailabilityListenerTest, AvailableObserverFirst) {
  MockMessageCallback mock_callback;
  MessageDemuxer::MessageWatch availability_watch =
      receiver_demuxer_->SetDefaultMessageTypeWatch(
          msgs::Type::kPresentationUrlAvailabilityRequest, &mock_callback);

  UrlAvailabilityListener listener({});

  std::string url1("https://example.com/foo.html");
  MockScreenObserver mock_observer;
  listener.AddObserver({url1}, controller_fake_clock_->Now(), &mock_observer);

  std::string screen_id("asdf");
  std::string friendly_name("turtle");
  ScreenInfo info1{screen_id, friendly_name, 1, receiver_endpoint_};
  listener.OnScreenAdded(info1, controller_fake_clock_->Now());

  msgs::PresentationUrlAvailabilityRequest request;
  uint64_t connection_id;
  EXPECT_CALL(mock_callback, OnStreamMessage(_, _, _, _, _, _))
      .WillOnce(
          ::testing::Invoke([&request, &connection_id](
                                uint64_t endpoint_id, uint64_t cid,
                                msgs::Type message_type, const uint8_t* buffer,
                                size_t buffer_size, platform::TimeDelta now) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url1}, request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer, OnScreensAvailable(url1, screen_id));
  EXPECT_CALL(mock_observer, OnScreensUnavailable(url1, screen_id)).Times(0);
  RunTasksUntilIdle();
}

TEST_F(UrlAvailabilityListenerTest, AvailableScreenFirst) {
  MockMessageCallback mock_callback;
  MessageDemuxer::MessageWatch availability_watch =
      receiver_demuxer_->SetDefaultMessageTypeWatch(
          msgs::Type::kPresentationUrlAvailabilityRequest, &mock_callback);

  std::string screen_id("asdf");
  std::string friendly_name("turtle");
  ScreenInfo info1{screen_id, friendly_name, 1, receiver_endpoint_};
  UrlAvailabilityListener listener({info1});

  std::string url1("https://example.com/foo.html");
  MockScreenObserver mock_observer;
  listener.AddObserver({url1}, controller_fake_clock_->Now(), &mock_observer);

  msgs::PresentationUrlAvailabilityRequest request;
  uint64_t connection_id;
  EXPECT_CALL(mock_callback, OnStreamMessage(_, _, _, _, _, _))
      .WillOnce(
          ::testing::Invoke([&request, &connection_id](
                                uint64_t endpoint_id, uint64_t cid,
                                msgs::Type message_type, const uint8_t* buffer,
                                size_t buffer_size, platform::TimeDelta now) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url1}, request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer, OnScreensAvailable(url1, screen_id));
  EXPECT_CALL(mock_observer, OnScreensUnavailable(url1, screen_id)).Times(0);
  RunTasksUntilIdle();
}

TEST_F(UrlAvailabilityListenerTest, Unavailable) {
  MockMessageCallback mock_callback;
  MessageDemuxer::MessageWatch availability_watch =
      receiver_demuxer_->SetDefaultMessageTypeWatch(
          msgs::Type::kPresentationUrlAvailabilityRequest, &mock_callback);

  std::string screen_id("asdf");
  std::string friendly_name("turtle");
  ScreenInfo info1{screen_id, friendly_name, 1, receiver_endpoint_};
  UrlAvailabilityListener listener({info1});

  std::string url1("https://example.com/foo.html");
  MockScreenObserver mock_observer;
  listener.AddObserver({url1}, controller_fake_clock_->Now(), &mock_observer);

  msgs::PresentationUrlAvailabilityRequest request;
  uint64_t connection_id;
  EXPECT_CALL(mock_callback, OnStreamMessage(_, _, _, _, _, _))
      .WillOnce(
          ::testing::Invoke([&request, &connection_id](
                                uint64_t endpoint_id, uint64_t cid,
                                msgs::Type message_type, const uint8_t* buffer,
                                size_t buffer_size, platform::TimeDelta now) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url1}, request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kNotCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer, OnScreensAvailable(url1, screen_id)).Times(0);
  EXPECT_CALL(mock_observer, OnScreensUnavailable(url1, screen_id));
  RunTasksUntilIdle();
}

TEST_F(UrlAvailabilityListenerTest, AvailabilityIsCached) {
  MockMessageCallback mock_callback;
  MessageDemuxer::MessageWatch availability_watch =
      receiver_demuxer_->SetDefaultMessageTypeWatch(
          msgs::Type::kPresentationUrlAvailabilityRequest, &mock_callback);

  std::string screen_id("asdf");
  std::string friendly_name("turtle");
  ScreenInfo info1{screen_id, friendly_name, 1, receiver_endpoint_};
  UrlAvailabilityListener listener({info1});

  std::string url1("https://example.com/foo.html");
  MockScreenObserver mock_observer1;
  listener.AddObserver({url1}, controller_fake_clock_->Now(), &mock_observer1);

  msgs::PresentationUrlAvailabilityRequest request;
  uint64_t connection_id;
  EXPECT_CALL(mock_callback, OnStreamMessage(_, _, _, _, _, _))
      .WillOnce(
          ::testing::Invoke([&request, &connection_id](
                                uint64_t endpoint_id, uint64_t cid,
                                msgs::Type message_type, const uint8_t* buffer,
                                size_t buffer_size, platform::TimeDelta now) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url1}, request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kNotCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnScreensAvailable(url1, screen_id)).Times(0);
  EXPECT_CALL(mock_observer1, OnScreensUnavailable(url1, screen_id));
  RunTasksUntilIdle();

  MockScreenObserver mock_observer2;
  EXPECT_CALL(mock_observer2, OnScreensAvailable(url1, screen_id)).Times(0);
  EXPECT_CALL(mock_observer2, OnScreensUnavailable(url1, screen_id));
  listener.AddObserver({url1}, controller_fake_clock_->Now(), &mock_observer2);
}

TEST_F(UrlAvailabilityListenerTest, AvailabilityCacheIsTransient) {
  MockMessageCallback mock_callback;
  MessageDemuxer::MessageWatch availability_watch =
      receiver_demuxer_->SetDefaultMessageTypeWatch(
          msgs::Type::kPresentationUrlAvailabilityRequest, &mock_callback);

  std::string screen_id("asdf");
  std::string friendly_name("turtle");
  ScreenInfo info1{screen_id, friendly_name, 1, receiver_endpoint_};
  UrlAvailabilityListener listener({info1});

  std::string url1("https://example.com/foo.html");
  MockScreenObserver mock_observer1;
  listener.AddObserver({url1}, controller_fake_clock_->Now(), &mock_observer1);

  msgs::PresentationUrlAvailabilityRequest request;
  uint64_t connection_id;
  EXPECT_CALL(mock_callback, OnStreamMessage(_, _, _, _, _, _))
      .WillOnce(
          ::testing::Invoke([&request, &connection_id](
                                uint64_t endpoint_id, uint64_t cid,
                                msgs::Type message_type, const uint8_t* buffer,
                                size_t buffer_size, platform::TimeDelta now) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url1}, request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kNotCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnScreensAvailable(url1, screen_id)).Times(0);
  EXPECT_CALL(mock_observer1, OnScreensUnavailable(url1, screen_id));
  RunTasksUntilIdle();

  listener.RemoveObserver({url1}, &mock_observer1);
  MockScreenObserver mock_observer2;
  EXPECT_CALL(mock_observer2, OnScreensAvailable(url1, screen_id)).Times(0);
  EXPECT_CALL(mock_observer2, OnScreensUnavailable(url1, screen_id)).Times(0);
  listener.AddObserver({url1}, controller_fake_clock_->Now(), &mock_observer2);
}

TEST_F(UrlAvailabilityListenerTest, PartiallyCachedAnswer) {
  MockMessageCallback mock_callback;
  MessageDemuxer::MessageWatch availability_watch =
      receiver_demuxer_->SetDefaultMessageTypeWatch(
          msgs::Type::kPresentationUrlAvailabilityRequest, &mock_callback);

  std::string screen_id("asdf");
  std::string friendly_name("turtle");
  ScreenInfo info1{screen_id, friendly_name, 1, receiver_endpoint_};
  UrlAvailabilityListener listener({info1});

  std::string url1("https://example.com/foo.html");
  std::string url2("https://example.com/bar.html");
  MockScreenObserver mock_observer1;
  listener.AddObserver({url1}, controller_fake_clock_->Now(), &mock_observer1);

  msgs::PresentationUrlAvailabilityRequest request;
  uint64_t connection_id;
  EXPECT_CALL(mock_callback, OnStreamMessage(_, _, _, _, _, _))
      .WillOnce(
          ::testing::Invoke([&request, &connection_id](
                                uint64_t endpoint_id, uint64_t cid,
                                msgs::Type message_type, const uint8_t* buffer,
                                size_t buffer_size, platform::TimeDelta now) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url1}, request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kNotCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnScreensAvailable(url1, screen_id)).Times(0);
  EXPECT_CALL(mock_observer1, OnScreensUnavailable(url1, screen_id));
  RunTasksUntilIdle();

  MockScreenObserver mock_observer2;
  EXPECT_CALL(mock_observer2, OnScreensAvailable(url1, screen_id)).Times(0);
  EXPECT_CALL(mock_observer2, OnScreensUnavailable(url1, screen_id));
  listener.AddObserver({url1, url2}, controller_fake_clock_->Now(),
                       &mock_observer2);

  EXPECT_CALL(mock_callback, OnStreamMessage(_, _, _, _, _, _))
      .WillOnce(
          ::testing::Invoke([&request, &connection_id](
                                uint64_t endpoint_id, uint64_t cid,
                                msgs::Type message_type, const uint8_t* buffer,
                                size_t buffer_size, platform::TimeDelta now) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url2}, request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kNotCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer2, OnScreensAvailable(url2, screen_id)).Times(0);
  EXPECT_CALL(mock_observer2, OnScreensUnavailable(url2, screen_id));
  RunTasksUntilIdle();
}

TEST_F(UrlAvailabilityListenerTest, MultipleOverlappingObservers) {
  MockMessageCallback mock_callback;
  MessageDemuxer::MessageWatch availability_watch =
      receiver_demuxer_->SetDefaultMessageTypeWatch(
          msgs::Type::kPresentationUrlAvailabilityRequest, &mock_callback);

  std::string screen_id("asdf");
  std::string friendly_name("turtle");
  ScreenInfo info1{screen_id, friendly_name, 1, receiver_endpoint_};
  UrlAvailabilityListener listener({info1});

  std::string url1("https://example.com/foo.html");
  std::string url2("https://example.com/bar.html");
  MockScreenObserver mock_observer1;
  listener.AddObserver({url1}, controller_fake_clock_->Now(), &mock_observer1);

  msgs::PresentationUrlAvailabilityRequest request;
  uint64_t connection_id;
  EXPECT_CALL(mock_callback, OnStreamMessage(_, _, _, _, _, _))
      .WillOnce(
          ::testing::Invoke([&request, &connection_id](
                                uint64_t endpoint_id, uint64_t cid,
                                msgs::Type message_type, const uint8_t* buffer,
                                size_t buffer_size, platform::TimeDelta now) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url1}, request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnScreensAvailable(url1, screen_id));
  EXPECT_CALL(mock_observer1, OnScreensUnavailable(url1, screen_id)).Times(0);
  RunTasksUntilIdle();

  controller_fake_clock_->now += platform::TimeDelta::FromSeconds(10);
  MockScreenObserver mock_observer2;
  EXPECT_CALL(mock_observer2, OnScreensAvailable(url1, screen_id));
  listener.AddObserver({url1, url2}, controller_fake_clock_->Now(),
                       &mock_observer2);

  EXPECT_CALL(mock_callback, OnStreamMessage(_, _, _, _, _, _))
      .WillOnce(
          ::testing::Invoke([&request, &connection_id](
                                uint64_t endpoint_id, uint64_t cid,
                                msgs::Type message_type, const uint8_t* buffer,
                                size_t buffer_size, platform::TimeDelta now) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url2}, request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kNotCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnScreensUnavailable(_, screen_id)).Times(0);
  EXPECT_CALL(mock_observer2, OnScreensAvailable(_, screen_id)).Times(0);
  EXPECT_CALL(mock_observer2, OnScreensUnavailable(url2, screen_id));
  RunTasksUntilIdle();
}

TEST_F(UrlAvailabilityListenerTest, RemoveObserver) {
  MockMessageCallback mock_callback;
  MessageDemuxer::MessageWatch availability_watch =
      receiver_demuxer_->SetDefaultMessageTypeWatch(
          msgs::Type::kPresentationUrlAvailabilityRequest, &mock_callback);

  std::string screen_id("asdf");
  std::string friendly_name("turtle");
  ScreenInfo info1{screen_id, friendly_name, 1, receiver_endpoint_};
  UrlAvailabilityListener listener({info1});

  std::string url1("https://example.com/foo.html");
  std::string url2("https://example.com/bar.html");
  MockScreenObserver mock_observer1;
  listener.AddObserver({url1}, controller_fake_clock_->Now(), &mock_observer1);

  msgs::PresentationUrlAvailabilityRequest request;
  uint64_t connection_id;
  EXPECT_CALL(mock_callback, OnStreamMessage(_, _, _, _, _, _))
      .WillOnce(
          ::testing::Invoke([&request, &connection_id](
                                uint64_t endpoint_id, uint64_t cid,
                                msgs::Type message_type, const uint8_t* buffer,
                                size_t buffer_size, platform::TimeDelta now) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url1}, request.urls);
  uint64_t url1_watch_id = request.watch_id;
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnScreensAvailable(url1, screen_id));
  EXPECT_CALL(mock_observer1, OnScreensUnavailable(url1, screen_id)).Times(0);
  RunTasksUntilIdle();

  controller_fake_clock_->now += platform::TimeDelta::FromSeconds(10);
  MockScreenObserver mock_observer2;
  listener.AddObserver({url1, url2}, controller_fake_clock_->Now(),
                       &mock_observer2);

  EXPECT_CALL(mock_callback, OnStreamMessage(_, _, _, _, _, _))
      .WillOnce(
          ::testing::Invoke([&request, &connection_id](
                                uint64_t endpoint_id, uint64_t cid,
                                msgs::Type message_type, const uint8_t* buffer,
                                size_t buffer_size, platform::TimeDelta now) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url2}, request.urls);

  listener.RemoveObserver({url1}, &mock_observer1);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kNotCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer2, OnScreensAvailable(_, screen_id)).Times(0);
  EXPECT_CALL(mock_observer2, OnScreensUnavailable(url2, screen_id));
  RunTasksUntilIdle();

  SendAvailabilityEvent(
      url1_watch_id, std::vector<std::string>{url1},
      std::vector<msgs::PresentationUrlAvailability>{msgs::kNotCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnScreensUnavailable(url1, screen_id)).Times(0);
  EXPECT_CALL(mock_observer2, OnScreensUnavailable(url1, screen_id));
  RunTasksUntilIdle();
}

TEST_F(UrlAvailabilityListenerTest, EventUpdate) {
  MockMessageCallback mock_callback;
  MessageDemuxer::MessageWatch availability_watch =
      receiver_demuxer_->SetDefaultMessageTypeWatch(
          msgs::Type::kPresentationUrlAvailabilityRequest, &mock_callback);

  std::string screen_id("asdf");
  std::string friendly_name("turtle");
  ScreenInfo info1{screen_id, friendly_name, 1, receiver_endpoint_};
  UrlAvailabilityListener listener({info1});

  std::string url1("https://example.com/foo.html");
  std::string url2("https://example.com/bar.html");
  MockScreenObserver mock_observer1;
  listener.AddObserver({url1, url2}, controller_fake_clock_->Now(),
                       &mock_observer1);

  msgs::PresentationUrlAvailabilityRequest request;
  uint64_t connection_id;
  EXPECT_CALL(mock_callback, OnStreamMessage(_, _, _, _, _, _))
      .WillOnce(
          ::testing::Invoke([&request, &connection_id](
                                uint64_t endpoint_id, uint64_t cid,
                                msgs::Type message_type, const uint8_t* buffer,
                                size_t buffer_size, platform::TimeDelta now) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ((std::vector<std::string>{url1, url2}), request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kCompatible,
                                                     msgs::kCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnScreensAvailable(url1, screen_id));
  EXPECT_CALL(mock_observer1, OnScreensAvailable(url2, screen_id));
  EXPECT_CALL(mock_observer1, OnScreensUnavailable(_, screen_id)).Times(0);
  RunTasksUntilIdle();

  controller_fake_clock_->now += platform::TimeDelta::FromSeconds(15);

  EXPECT_CALL(mock_callback, OnStreamMessage(_, _, _, _, _, _)).Times(0);
  SendAvailabilityEvent(
      request.watch_id, std::vector<std::string>{url2},
      std::vector<msgs::PresentationUrlAvailability>{msgs::kNotCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnScreensUnavailable(url2, screen_id));
  RunTasksUntilIdle();
}

TEST_F(UrlAvailabilityListenerTest, RefreshWatches) {
  MockMessageCallback mock_callback;
  MessageDemuxer::MessageWatch availability_watch =
      receiver_demuxer_->SetDefaultMessageTypeWatch(
          msgs::Type::kPresentationUrlAvailabilityRequest, &mock_callback);

  std::string screen_id("asdf");
  std::string friendly_name("turtle");
  ScreenInfo info1{screen_id, friendly_name, 1, receiver_endpoint_};
  UrlAvailabilityListener listener({info1});

  std::string url1("https://example.com/foo.html");
  MockScreenObserver mock_observer1;
  listener.AddObserver({url1}, controller_fake_clock_->Now(), &mock_observer1);

  msgs::PresentationUrlAvailabilityRequest request;
  uint64_t connection_id;
  EXPECT_CALL(mock_callback, OnStreamMessage(_, _, _, _, _, _))
      .WillOnce(
          ::testing::Invoke([&request, &connection_id](
                                uint64_t endpoint_id, uint64_t cid,
                                msgs::Type message_type, const uint8_t* buffer,
                                size_t buffer_size, platform::TimeDelta now) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url1}, request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnScreensAvailable(url1, screen_id));
  EXPECT_CALL(mock_observer1, OnScreensUnavailable(_, screen_id)).Times(0);
  RunTasksUntilIdle();

  controller_fake_clock_->now += platform::TimeDelta::FromSeconds(60);

  EXPECT_CALL(mock_callback, OnStreamMessage(_, _, _, _, _, _))
      .WillOnce(
          ::testing::Invoke([&request, &connection_id](
                                uint64_t endpoint_id, uint64_t cid,
                                msgs::Type message_type, const uint8_t* buffer,
                                size_t buffer_size, platform::TimeDelta now) {
            connection_id = cid;
            ssize_t result = msgs::DecodePresentationUrlAvailabilityRequest(
                buffer, buffer_size, &request);
            OSP_DCHECK_GT(result, 0);
            return result;
          }));
  listener.RefreshWatches(controller_fake_clock_->Now());
  RunTasksUntilIdle();

  EXPECT_EQ(std::vector<std::string>{url1}, request.urls);
  SendAvailabilityResponse(
      request,
      std::vector<msgs::PresentationUrlAvailability>{msgs::kNotCompatible},
      fake_factory_->GetIncomingStream(receiver_endpoint_, connection_id));

  EXPECT_CALL(mock_observer1, OnScreensUnavailable(url1, screen_id));
  RunTasksUntilIdle();
}

}  // namespace presentation
}  // namespace openscreen
