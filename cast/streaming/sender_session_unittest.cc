// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/sender_session.h"

#include <utility>

#include "absl/strings/str_cat.h"
#include "cast/streaming/capture_recommendations.h"
#include "cast/streaming/mock_environment.h"
#include "cast/streaming/session_base.h"
#include "cast/streaming/session_test_support.h"
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
constexpr char kMalformedAnswerMessage[] = R"({
  "type": "ANSWER",
  "seqNum": 1,
  "answer": {
    "castMode": "mirroring",
    "udpPort": 1234,
    "sendIndexes": [1, 3],
    "ssrcs": [1, 2]
})";

constexpr char kValidJsonInvalidFormatAnswerMessage[] = R"({
  "type": "ANSWER",
  "seqNum": 1,
  "answer-2": {
    "castMode": "mirroring",
    "udpPort": 1234,
    "sendIndexes": [1, 3],
    "ssrcs": [1, 2]
  }
})";

constexpr char kMissingAnswerMessage[] = R"({
  "type": "ANSWER",
  "seqNum": 1
})";

constexpr char kInvalidSequenceNumberMessage[] = R"({
  "type": "ANSWER",
  "seqNum": "not actually a number"
})";

constexpr char kUnknownTypeMessage[] = R"({
  "type": "ANSWER_VERSION_2",
  "seqNum": 1
})";

constexpr char kInvalidTypeMessage[] = R"({
  "type": 39,
  "seqNum": 1
})";

const AudioConfig kAudioConfigInvalidChannels{
    AudioCodec::kAac, -1 /* channels */, 44000 /* bit_rate */,
    96000 /* rtp_timebase */
};

const AudioConfig kAudioConfigValid{
    AudioCodec::kOpus, 5 /* channels */, 32000 /* bit_rate */,
    96000 /* rtp_timebase */
};

const VideoConfig kVideoConfigMissingResolutions{
    VideoCodec::kHevc,
    FrameRate{60, 1},
    300000 /* max_bit_rate */,
    96000 /* rtp_timebase */,
    "fec" /* protection */,
    "base" /* profile */,
    "baseline" /* level */,
    "no_recovery" /* error_recovery_mode */,
    std::vector<DisplayResolution>{}};

const VideoConfig kVideoConfigInvalid{
    VideoCodec::kHevc,
    FrameRate{60, 1},
    -300000 /* max_bit_rate */,
    96000 /* rtp_timebase */,
    "fec" /* protection */,
    "base" /* profile */,
    "level" /* level */,
    "recovery" /* error_recovery_mode */,
    std::vector<DisplayResolution>{DisplayResolution{1920, 1080},
                                   DisplayResolution{1280, 720}}};

const VideoConfig kVideoConfigValid{
    VideoCodec::kHevc,
    FrameRate{60, 1},
    300000 /* max_bit_rate */,
    96000 /* rtp_timebase */,
    "fec" /* protection */,
    "base" /* profile */,
    "level" /* level */,
    "recovery" /* error_recovery_mode */,
    std::vector<DisplayResolution>{DisplayResolution{1280, 720},
                                   DisplayResolution{1920, 1080}}};

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
    session_ = std::make_unique<SenderSession>(IPAddress::kV4LoopbackAddress(),
                                               &client_, environment_.get(),
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

TEST_F(SenderSessionTest, ComplainsIfNoConfigsToOffer) {
  EXPECT_CALL(client_, OnError(session_.get(), _));
  session_->Negotiate(std::vector<AudioConfig>{}, std::vector<VideoConfig>{});
}

TEST_F(SenderSessionTest, ComplainsIfInvalidAudioConfig) {
  EXPECT_CALL(client_, OnError(session_.get(), _));
  session_->Negotiate(std::vector<AudioConfig>{kAudioConfigInvalidChannels},
                      std::vector<VideoConfig>{});
}

TEST_F(SenderSessionTest, ComplainsIfInvalidVideoConfig) {
  EXPECT_CALL(client_, OnError(session_.get(), _));
  session_->Negotiate(std::vector<AudioConfig>{},
                      std::vector<VideoConfig>{kVideoConfigInvalid});
}

TEST_F(SenderSessionTest, ComplainsIfMissingResolutions) {
  EXPECT_CALL(client_, OnError(session_.get(), _));
  session_->Negotiate(std::vector<AudioConfig>{},
                      std::vector<VideoConfig>{kVideoConfigMissingResolutions});
}

TEST_F(SenderSessionTest, SendsOfferMessage) {
  session_->Negotiate(std::vector<AudioConfig>{kAudioConfigValid},
                      std::vector<VideoConfig>{kVideoConfigValid});

  const auto& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());

  auto message_body = json::Parse(messages[0]);
  ASSERT_TRUE(message_body.is_value());
  const Json::Value offer = std::move(message_body.value());
  EXPECT_EQ("OFFER", offer["type"].asString());
  EXPECT_LT(0, offer["seqNum"].asInt());

  const Json::Value& offer_body = offer["offer"];
  ASSERT_FALSE(offer_body.isNull());
  ASSERT_TRUE(offer_body.isObject());
  EXPECT_EQ("mirroring", offer_body["castMode"].asString());
  EXPECT_EQ(false, offer_body["receiverGetStatus"].asBool());

  const Json::Value& streams = offer_body["supportedStreams"];
  EXPECT_TRUE(streams.isArray());
  EXPECT_EQ(2u, streams.size());

  const Json::Value& audio_stream = streams[0];
  EXPECT_EQ("opus", audio_stream["codecName"].asString());
  EXPECT_EQ(0, audio_stream["index"].asInt());
  EXPECT_EQ(32u, audio_stream["aesKey"].asString().length());
  EXPECT_EQ(32u, audio_stream["aesIvMask"].asString().length());
  EXPECT_EQ(5, audio_stream["channels"].asInt());
  EXPECT_LT(0u, audio_stream["ssrc"].asUInt());
  EXPECT_EQ(127, audio_stream["rtpPayloadType"].asInt());

  const Json::Value& video_stream = streams[1];
  EXPECT_EQ("hevc", video_stream["codecName"].asString());
  EXPECT_EQ(1, video_stream["index"].asInt());
  EXPECT_EQ(32u, video_stream["aesKey"].asString().length());
  EXPECT_EQ(32u, video_stream["aesIvMask"].asString().length());
  EXPECT_EQ(1, video_stream["channels"].asInt());
  EXPECT_LT(0u, video_stream["ssrc"].asUInt());
  EXPECT_EQ(96, video_stream["rtpPayloadType"].asInt());
}

TEST_F(SenderSessionTest, HandlesValidAnswer) {
  session_->Negotiate(std::vector<AudioConfig>{kAudioConfigValid},
                      std::vector<VideoConfig>{kVideoConfigValid});

  const auto& messages = message_port_->posted_messages();
  ASSERT_EQ(1u, messages.size());
  auto message_body = json::Parse(messages[0]);
  ASSERT_TRUE(message_body.is_value());
  const Json::Value offer = std::move(message_body.value());
  EXPECT_EQ("OFFER", offer["type"].asString());
  EXPECT_LT(0, offer["seqNum"].asInt());

  const Json::Value& offer_body = offer["offer"];
  ASSERT_FALSE(offer_body.isNull());
  ASSERT_TRUE(offer_body.isObject());
  const Json::Value& streams = offer_body["supportedStreams"];
  EXPECT_TRUE(streams.isArray());
  EXPECT_EQ(2u, streams.size());

  const Json::Value& audio_stream = streams[0];
  const int audio_index = audio_stream["index"].asInt();
  const int audio_ssrc = audio_stream["ssrc"].asUInt();

  const Json::Value& video_stream = streams[1];
  const int video_index = video_stream["index"].asInt();
  const int video_ssrc = video_stream["ssrc"].asUInt();

  std::string answer = absl::StrCat(R"({
                                    "type": "ANSWER",
                                    "seqNum": )",
                                    offer["seqNum"].asInt(), R"(,
                                    "answer": {
                                      "castMode": "mirroring",
                                      "udpPort": 1234,
                                      "sendIndexes": [)",
                                    audio_index, ",", video_index, R"(],
                                    "ssrcs": [)",
                                    audio_ssrc + 1, ",", video_ssrc + 1, R"(]
    }
  })");

  // We should have responded with an on negotiated call
  EXPECT_CALL(client_, OnNegotiated(session_.get(), _, _));
  message_port_->ReceiveMessage(answer);
}

TEST_F(SenderSessionTest, HandlesMalformedAnswer) {
  session_->Negotiate(std::vector<AudioConfig>{kAudioConfigValid},
                      std::vector<VideoConfig>{kVideoConfigValid});

  // Note that unlike when we simply don't select any streams, when the answer
  // is actually completely invalid we call OnError.
  EXPECT_CALL(client_,
              OnError(session_.get(), Error(Error::Code::kJsonParseError)));

  message_port_->ReceiveMessage(kMalformedAnswerMessage);
}

TEST_F(SenderSessionTest, HandlesImproperlyFormattedAnswer) {
  session_->Negotiate(std::vector<AudioConfig>{kAudioConfigValid},
                      std::vector<VideoConfig>{kVideoConfigValid});

  EXPECT_CALL(client_,
              OnError(session_.get(), Error(Error::Code::kJsonParseError,
                                            "Failed to parse answer")));
  message_port_->ReceiveMessage(kValidJsonInvalidFormatAnswerMessage);
}

TEST_F(SenderSessionTest, HandlesNullAnswer) {
  session_->Negotiate(std::vector<AudioConfig>{kAudioConfigValid},
                      std::vector<VideoConfig>{kVideoConfigValid});

  EXPECT_CALL(client_,
              OnError(session_.get(), Error(Error::Code::kJsonParseError,
                                            "Failed to parse answer")));

  message_port_->ReceiveMessage(kMissingAnswerMessage);
}

TEST_F(SenderSessionTest, HandlesInvalidSequenceNumber) {
  session_->Negotiate(std::vector<AudioConfig>{kAudioConfigValid},
                      std::vector<VideoConfig>{kVideoConfigValid});

  // We should just discard messages with an invalid sequence number.
  message_port_->ReceiveMessage(kInvalidSequenceNumberMessage);
}

TEST_F(SenderSessionTest, HandlesUnknownTypeMessage) {
  session_->Negotiate(std::vector<AudioConfig>{kAudioConfigValid},
                      std::vector<VideoConfig>{kVideoConfigValid});

  // We should just discard messages with an unknown message type.
  message_port_->ReceiveMessage(kUnknownTypeMessage);
}

TEST_F(SenderSessionTest, HandlesInvalidTypeMessage) {
  session_->Negotiate(std::vector<AudioConfig>{kAudioConfigValid},
                      std::vector<VideoConfig>{kVideoConfigValid});

  // We should just discard messages with an invalid message type.
  message_port_->ReceiveMessage(kInvalidTypeMessage);
}

TEST_F(SenderSessionTest, DoesntCrashOnMessagePortError) {
  session_->Negotiate(std::vector<AudioConfig>{kAudioConfigValid},
                      std::vector<VideoConfig>{kVideoConfigValid});

  message_port_->ReceiveError(Error(Error::Code::kUnknownError));
}

}  // namespace cast
}  // namespace openscreen
