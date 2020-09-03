// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/sender_session.h"

#include <utility>

#include "cast/streaming/capture_recommendations.h"
#include "cast/streaming/mock_environment.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/base/ip_address.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"
#include "util/chrono_helpers.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace openscreen {
namespace cast {

namespace {

constexpr char kValidJsonInvalidFormatAnswerMessage[] = R"({
  "type": "ANSWER",
  "seqNum": 1337,
  "offer": {
    "castMode": "mirroring",
    "senderGetStatus": true,
    "supportedStreams": "anything"
  }
})";

constexpr char kMissingAnswerMessage[] = R"({
  "type": "ANSWER",
  "seqNum": 1337
})";

constexpr char kInvalidSequenceNumberMessage[] = R"({
  "type": "ANSWER",
  "seqNum": "not actually a number"
})";

constexpr char kUnknownTypeMessage[] = R"({
  "type": "ANSWER_VERSION_2",
  "seqNum": 1337
})";

constexpr char kInvalidTypeMessage[] = R"({
  "type": 39,
  "seqNum": 1337
})";

// TODO: Move to shared testt.
class SimpleMessagePort : public MessagePort {
 public:
  ~SimpleMessagePort() override {}
  void SetClient(MessagePort::Client* client) override { client_ = client; }

  void ReceiveMessage(const std::string& message) {
    ASSERT_NE(client_, nullptr);
    client_->OnMessage("sender-id", "namespace", message);
  }

  void ReceiveError(Error error) {
    ASSERT_NE(client_, nullptr);
    client_->OnError(error);
  }

  void PostMessage(const std::string& sender_id,
                   const std::string& message_namespace,
                   const std::string& message) override {
    posted_messages_.emplace_back(std::move(message));
  }

  MessagePort::Client* client() const { return client_; }
  const std::vector<std::string> posted_messages() const {
    return posted_messages_;
  }

 private:
  MessagePort::Client* client_ = nullptr;
  std::vector<std::string> posted_messages_;
};

class FakeClient : public SenderSession::Client {
 public:
  MOCK_METHOD(void,
              OnNegotiated,
              (const SenderSession*,
               SenderSession::ConfiguredSenders,
               capture_recommendations::Recommendations),
              (override));
  MOCK_METHOD(void, OnError, (const SenderSession*, Error error), (override));
};

}  // namespace

class SenderSessionTest : public ::testing::Test {
 public:
  SenderSessionTest() : clock_(Clock::time_point{}), task_runner_(&clock_) {}

  std::unique_ptr<MockEnvironment> MakeEnvironment() {
    auto environment_ = std::make_unique<NiceMock<MockEnvironment>>(
        &FakeClock::now, &task_runner_);
    ON_CALL(*environment_, GetBoundLocalEndpoint())
        .WillByDefault(
            Return(IPEndpoint{IPAddress::Parse("127.0.0.1").value(), 12345}));
    return environment_;
  }

  void SetUp() {
    message_port_ = std::make_unique<SimpleMessagePort>();
    environment_ = MakeEnvironment();
    session_ = std::make_unique<SenderSession>(&client_, environment_.get(),
                                               message_port_.get());
  }

 protected:
  StrictMock<FakeClient> client_;
  FakeClock clock_;
  std::unique_ptr<MockEnvironment> environment_;
  std::unique_ptr<SimpleMessagePort> message_port_;
  std::unique_ptr<SenderSession> session_;
  FakeTaskRunner task_runner_;
};

TEST_F(SenderSessionTest, RegistersSelfOnMessagePort) {
  EXPECT_EQ(message_port_->client(), session_.get());
}

TEST_F(SenderSessionTest, HandlesMalformedAnswer) {
  // Note that unlike when we simply don't select any streams, when the answer
  // is actually completely invalid we call OnError.
  EXPECT_CALL(client_,
              OnError(session_.get(), Error(Error::Code::kJsonParseError)));

  // TODO: make malformed answer
  message_port_->ReceiveMessage("");
}

TEST_F(SenderSessionTest, HandlesImproperlyFormattedAnswer) {
  EXPECT_CALL(client_,
              OnError(session_.get(), Error(Error::Code::kJsonParseError,
                                            "Failed to parse answer")));
  message_port_->ReceiveMessage(kValidJsonInvalidFormatAnswerMessage);
}

TEST_F(SenderSessionTest, HandlesNullAnswer) {
  EXPECT_CALL(client_,
              OnError(session_.get(), Error(Error::Code::kJsonParseError,
                                            "Failed to parse answer")));

  message_port_->ReceiveMessage(kMissingAnswerMessage);
}

TEST_F(SenderSessionTest, HandlesInvalidSequenceNumber) {
  // We should just discard messages with an invalid sequence number.
  message_port_->ReceiveMessage(kInvalidSequenceNumberMessage);
}

TEST_F(SenderSessionTest, HandlesUnknownTypeMessage) {
  // We should just discard messages with an unknown message type.
  message_port_->ReceiveMessage(kUnknownTypeMessage);
}

TEST_F(SenderSessionTest, HandlesInvalidTypeMessage) {
  // We should just discard messages with an invalid message type.
  message_port_->ReceiveMessage(kInvalidTypeMessage);
}

TEST_F(SenderSessionTest, DoesntCrashOnMessagePortError) {
  message_port_->ReceiveError(Error(Error::Code::kUnknownError));
}

}  // namespace cast
}  // namespace openscreen
