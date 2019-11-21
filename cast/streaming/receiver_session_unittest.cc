// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/receiver_session.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::StrictMock;

// TODO: fix bad variant accesses so tests fail more usefully
namespace cast {
namespace streaming {

namespace {

const std::string kValidOfferMessage = R"({
  "type": "OFFER",
  "seqNum": 1337,
  "offer": {
  "castMode": "mirroring",
 	"receiverGetStatus": true,
 	"supportedStreams": [{
 			"index": 31337,
 			"type": "video_source",
 			"codecName": "unknownCodec",
 			"rtpProfile": "cast",
 			"rtpPayloadType": 127,
 			"ssrc": 19088743,
 			"maxFrameRate": "60000/1000",
 			"timeBase": "1/90000",
 			"maxBitRate": 5000000,
 			"profile": "main",
 			"level": "4",
 			"resolutions": [{
 					"width": 1280,
 					"height": 720
 				}
 			]
 		},
    {
      "index": 31338,
      "type": "video_source",
      "codecName": "vp8",
      "rtpProfile": "cast",
      "rtpPayloadType": 127,
      "ssrc": 19088743,
      "maxFrameRate": "60000/1000",
      "timeBase": "1/90000",
      "maxBitRate": 5000000,
      "profile": "main",
      "level": "4",
      "resolutions": [{
          "width": 1280,
          "height": 720
        }
      ]
    },
 		{
 			"index": 1337,
 			"type": "audio_source",
 			"codecName": "opus",
 			"rtpProfile": "cast",
 			"rtpPayloadType": 97,
 			"ssrc": 19088743,
 			"bitRate": 124000,
 			"timeBase": "1/48000",
 			"channels": 2
 		}
 	]
 }}
)";

class FakeMessagePort : public MessagePort {
 public:
  MOCK_METHOD(void, SetClient, (MessagePort::Client*), (override));
  MOCK_METHOD(void, PostMessage, (absl::string_view), (override));
  MOCK_METHOD(void, Close, (), (override));
};

class SimpleMessagePort : public MessagePort {
 public:
  void SetClient(MessagePort::Client* client) { client_ = client; }

  void ReceiveMessage(absl::string_view message) {
    ASSERT_NE(client_, nullptr);
    client_->OnMessage(message);
  }

  void ReceiveError(openscreen::Error error) {
    ASSERT_NE(client_, nullptr);
    client_->OnError(error);
  }

  void PostMessage(absl::string_view message) {
    posted_messages_.emplace_back(message);
  }

  void Close() { is_closed_ = true; }

  MessagePort::Client* client_ = nullptr;
  bool is_closed_ = false;
  std::vector<std::string> posted_messages_;
};

class FakeClient : public ReceiverSession::Client {
 public:
  MOCK_METHOD(void,
              OnNegotiated,
              (ReceiverSession*, ReceiverSession::ConfiguredReceivers));
  MOCK_METHOD(void, OnError, (ReceiverSession*, openscreen::Error error));
};

}  // namespace

TEST(ReceiverSessionTest, RegistersAndDeregistersSelfOnMessagePump) {
  SimpleMessagePort message_port;
  StrictMock<FakeClient> client;

  auto session = std::make_unique<ReceiverSession>(
      &client, &message_port, ReceiverSession::Preferences{});
  EXPECT_EQ(message_port.client_, session.get());

  session.reset();
  EXPECT_EQ(message_port.client_, nullptr);
}

TEST(ReceiverSessionTest, DefaultPreferencesChoosesSupportedEncoding) {
  SimpleMessagePort message_port;
  StrictMock<FakeClient> client;
  ReceiverSession session(&client, &message_port,
                          ReceiverSession::Preferences{});

  message_port.ReceiveMessage(kValidOfferMessage);
}

TEST(ReceiverSessionTest, CustomCodecPreferencesResultInCodecSelection) {}
}  // namespace streaming
}  // namespace cast
