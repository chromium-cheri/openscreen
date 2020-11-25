// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/session_messager.h"

#include "cast/streaming/testing/message_pipe.h"
#include "gtest/gtest.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"

namespace openscreen {
namespace cast {

using ::testing::ElementsAre;

namespace {

constexpr char kSenderId[] = "sender-12345";
constexpr char kReceiverId[] = "receiver-12345";

// Generally the messages are inlined below, with the exception of the Offer,
// simply because it is massive.
Offer kExampleOffer{
    CastMode::kMirroring,
    false,
    {AudioStream{Stream{0,
                        Stream::Type::kAudioSource,
                        2,
                        "opus",
                        RtpPayloadType::kAudioOpus,
                        12344442,
                        std::chrono::milliseconds{2000},
                        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
                        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
                        false,
                        "",
                        48000},
                 1400}},
    {VideoStream{Stream{1,
                        Stream::Type::kVideoSource,
                        1,
                        "vp8",
                        RtpPayloadType::kVideoVp8,
                        12344444,
                        std::chrono::milliseconds{2000},
                        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
                        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
                        false,
                        "",
                        90000},
                 SimpleFraction{30, 1},
                 3000000,
                 "",
                 "",
                 "",
                 {Resolution{640, 480}},
                 ""}}};

struct SessionMessageStore {
 public:
  SenderSessionMessager::ReplyCallback GetReplyCallback() {
    return [this](ReceiverMessage message) {
      receiver_messages.push_back(std::move(message));
    };
  }

  ReceiverSessionMessager::RequestCallback GetRequestCallback() {
    return [this](SenderMessage message) {
      sender_messages.push_back(std::move(message));
    };
  }

  SessionMessager::ErrorCallback GetErrorCallback() {
    return [this](Error error) { errors.push_back(std::move(error)); };
  }

  std::vector<SenderMessage> sender_messages;
  std::vector<ReceiverMessage> receiver_messages;
  std::vector<Error> errors;
};
}  // namespace

class SessionMessagerTest : public ::testing::Test {
 public:
  SessionMessagerTest()
      : clock_{Clock::now()},
        task_runner_(&clock_),
        message_store_(),
        pipe_(kSenderId, kReceiverId),
        receiver_messager_(pipe_.right(),
                           kReceiverId,
                           message_store_.GetErrorCallback()),
        sender_messager_(pipe_.left(),
                         kSenderId,
                         kReceiverId,
                         message_store_.GetErrorCallback(),
                         &task_runner_)

  {}

  void SetUp() override {
    sender_messager_.SetHandler(ReceiverMessage::Type::kRpc,
                                message_store_.GetReplyCallback());
    receiver_messager_.SetHandler(SenderMessage::Type::kOffer,
                                  message_store_.GetRequestCallback());
    receiver_messager_.SetHandler(SenderMessage::Type::kGetStatus,
                                  message_store_.GetRequestCallback());
    receiver_messager_.SetHandler(SenderMessage::Type::kGetCapabilities,
                                  message_store_.GetRequestCallback());
    receiver_messager_.SetHandler(SenderMessage::Type::kRpc,
                                  message_store_.GetRequestCallback());
  }

 protected:
  FakeClock clock_;
  FakeTaskRunner task_runner_;
  SessionMessageStore message_store_;
  MessagePipe pipe_;
  ReceiverSessionMessager receiver_messager_;
  SenderSessionMessager sender_messager_;

  std::vector<Error> receiver_errors_;
  std::vector<Error> sender_errors_;
};

TEST_F(SessionMessagerTest, RpcMessaging) {
  sender_messager_.SendOutboundMessage(
      SenderMessage{SenderMessage::Type::kRpc, 123, true /* valid */,
                    std::string("all your base are belong to us")});

  ASSERT_EQ(1u, message_store_.sender_messages.size());
  ASSERT_EQ(0u, message_store_.receiver_messages.size());
  EXPECT_EQ(SenderMessage::Type::kRpc, message_store_.sender_messages[0].type);
  EXPECT_TRUE(message_store_.sender_messages[0].valid);
  EXPECT_EQ("all your base are belong to us",
            absl::get<std::string>(message_store_.sender_messages[0].body));

  message_store_.sender_messages.clear();
  receiver_messager_.SendMessage(ReceiverMessage{ReceiverMessage::Type::kRpc,
                                                 123, true /* valid */,
                                                 std::string("nuh uh")});

  ASSERT_EQ(0u, message_store_.sender_messages.size());
  ASSERT_EQ(1u, message_store_.receiver_messages.size());
  EXPECT_EQ(ReceiverMessage::Type::kRpc,
            message_store_.receiver_messages[0].type);
  EXPECT_TRUE(message_store_.receiver_messages[0].valid);
  EXPECT_EQ("nuh uh",
            absl::get<std::string>(message_store_.receiver_messages[0].body));
}

TEST_F(SessionMessagerTest, StatusMessaging) {
  sender_messager_.SendRequest(
      SenderMessage{SenderMessage::Type::kGetStatus, 3123, true /* valid */},
      ReceiverMessage::Type::kStatusResponse,
      message_store_.GetReplyCallback());

  ASSERT_EQ(1u, message_store_.sender_messages.size());
  ASSERT_EQ(0u, message_store_.receiver_messages.size());
  EXPECT_EQ(SenderMessage::Type::kGetStatus,
            message_store_.sender_messages[0].type);
  EXPECT_TRUE(message_store_.sender_messages[0].valid);

  message_store_.sender_messages.clear();
  receiver_messager_.SendMessage(ReceiverMessage{
      ReceiverMessage::Type::kStatusResponse, 3123, true /* valid */,
      ReceiverWifiStatus{-5.7, std::vector<int32_t>{1200, 1300, 1250}}});

  ASSERT_EQ(0u, message_store_.sender_messages.size());
  ASSERT_EQ(1u, message_store_.receiver_messages.size());
  EXPECT_EQ(ReceiverMessage::Type::kStatusResponse,
            message_store_.receiver_messages[0].type);
  EXPECT_TRUE(message_store_.receiver_messages[0].valid);

  const auto& status =
      absl::get<ReceiverWifiStatus>(message_store_.receiver_messages[0].body);
  EXPECT_DOUBLE_EQ(-5.7, status.wifi_snr);
  EXPECT_THAT(status.wifi_speed, ElementsAre(1200, 1300, 1250));
}

TEST_F(SessionMessagerTest, CapabilitiesMessaging) {
  sender_messager_.SendRequest(
      SenderMessage{SenderMessage::Type::kGetCapabilities, 1337,
                    true /* valid */},
      ReceiverMessage::Type::kCapabilitiesResponse,
      message_store_.GetReplyCallback());

  ASSERT_EQ(1u, message_store_.sender_messages.size());
  ASSERT_EQ(0u, message_store_.receiver_messages.size());
  EXPECT_EQ(SenderMessage::Type::kGetCapabilities,
            message_store_.sender_messages[0].type);
  EXPECT_TRUE(message_store_.sender_messages[0].valid);

  message_store_.sender_messages.clear();
  receiver_messager_.SendMessage(
      ReceiverMessage{ReceiverMessage::Type::kCapabilitiesResponse, 1337,
                      true /* valid */, ReceiverCapability{47, {"ac3", "4k"}}});

  ASSERT_EQ(0u, message_store_.sender_messages.size());
  ASSERT_EQ(1u, message_store_.receiver_messages.size());
  EXPECT_EQ(ReceiverMessage::Type::kCapabilitiesResponse,
            message_store_.receiver_messages[0].type);
  EXPECT_TRUE(message_store_.receiver_messages[0].valid);

  const auto& capability =
      absl::get<ReceiverCapability>(message_store_.receiver_messages[0].body);
  EXPECT_DOUBLE_EQ(47, capability.remoting_version);
  EXPECT_THAT(capability.media_capabilities, ElementsAre("ac3", "4k"));
}

TEST_F(SessionMessagerTest, OfferAnswerMessaging) {
  sender_messager_.SendRequest(SenderMessage{SenderMessage::Type::kOffer, 42,
                                             true /* valid */, kExampleOffer},
                               ReceiverMessage::Type::kAnswer,
                               message_store_.GetReplyCallback());

  ASSERT_EQ(1u, message_store_.sender_messages.size());
  ASSERT_EQ(0u, message_store_.receiver_messages.size());
  EXPECT_EQ(SenderMessage::Type::kOffer,
            message_store_.sender_messages[0].type);
  EXPECT_TRUE(message_store_.sender_messages[0].valid);
  message_store_.sender_messages.clear();

  EXPECT_TRUE(receiver_messager_
                  .SendMessage(ReceiverMessage{
                      ReceiverMessage::Type::kAnswer, 41, true /* valid */,
                      Answer{1234, {0, 1}, {12344443, 12344445}}})
                  .ok());
  // A stale answer (for offer 41) should get ignored:
  ASSERT_EQ(0u, message_store_.sender_messages.size());
  ASSERT_EQ(0u, message_store_.receiver_messages.size());

  receiver_messager_.SendMessage(
      ReceiverMessage{ReceiverMessage::Type::kAnswer, 42, true /* valid */,
                      Answer{1234, {0, 1}, {12344443, 12344445}}});
  ASSERT_EQ(0u, message_store_.sender_messages.size());
  ASSERT_EQ(1u, message_store_.receiver_messages.size());
  EXPECT_EQ(ReceiverMessage::Type::kAnswer,
            message_store_.receiver_messages[0].type);
  EXPECT_TRUE(message_store_.receiver_messages[0].valid);

  const auto& answer =
      absl::get<Answer>(message_store_.receiver_messages[0].body);
  EXPECT_EQ(1234, answer.udp_port);

  EXPECT_THAT(answer.send_indexes, ElementsAre(0, 1));
  EXPECT_THAT(answer.ssrcs, ElementsAre(12344443, 12344445));
}

TEST_F(SessionMessagerTest, UnexpectedMessagesAreIgnored) {
  receiver_messager_.SendMessage(ReceiverMessage{
      ReceiverMessage::Type::kStatusResponse, 3123, true /* valid */,
      ReceiverWifiStatus{-5.7, std::vector<int32_t>{1200, 1300, 1250}}});

  // The message gets dropped and thus won't be in the store.
  EXPECT_TRUE(message_store_.sender_messages.empty());
  EXPECT_TRUE(message_store_.receiver_messages.empty());
}

TEST_F(SessionMessagerTest, UnknownMessageTypesDontGetSent) {
  receiver_messager_.SendMessage(
      ReceiverMessage{ReceiverMessage::Type::kUnknown, 3123, true /* valid */});
  sender_messager_.SendOutboundMessage(
      SenderMessage{SenderMessage::Type::kUnknown, 123, true /* valid */});

  // The message gets dropped and thus won't be in the store.
  EXPECT_TRUE(message_store_.sender_messages.empty());
  EXPECT_TRUE(message_store_.receiver_messages.empty());
}

TEST_F(SessionMessagerTest, ReceiverCannotSendFirst) {
  const Error error = receiver_messager_.SendMessage(ReceiverMessage{
      ReceiverMessage::Type::kStatusResponse, 3123, true /* valid */,
      ReceiverWifiStatus{-5.7, std::vector<int32_t>{1200, 1300, 1250}}});

  EXPECT_EQ(Error::Code::kInitializationFailure, error.code());
}

TEST_F(SessionMessagerTest, ErrorMessageLoggedIfTimeout) {
  sender_messager_.SendRequest(
      SenderMessage{SenderMessage::Type::kGetStatus, 3123, true /* valid */},
      ReceiverMessage::Type::kStatusResponse,
      message_store_.GetReplyCallback());

  ASSERT_EQ(1u, message_store_.sender_messages.size());
  ASSERT_EQ(0u, message_store_.receiver_messages.size());

  clock_.Advance(std::chrono::seconds(10));
  ASSERT_EQ(1u, message_store_.sender_messages.size());
  ASSERT_EQ(1u, message_store_.receiver_messages.size());
  EXPECT_EQ(3123, message_store_.receiver_messages[0].sequence_number);
  EXPECT_EQ(ReceiverMessage::Type::kStatusResponse,
            message_store_.receiver_messages[0].type);
  EXPECT_FALSE(message_store_.receiver_messages[0].valid);
}

}  // namespace cast
}  // namespace openscreen
