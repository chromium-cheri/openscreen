// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/receiver_session.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::_;
using ::testing::StrictMock;

namespace cast {
namespace streaming {

namespace {

class FakeMessagePort : public MessagePort {
 public:
  MOCK_METHOD(void, SetClient, (MessagePort::Client*), (override));
  MOCK_METHOD(void, PostMessage, (absl::string_view), (override));
  MOCK_METHOD(void, Close, (), (override));
};

class FakeClient : public ReceiverSession::Client {
 public:
  MOCK_METHOD(void,
              OnNegotiated,
              (ReceiverSession*, ReceiverSession::ConfiguredReceivers));
  MOCK_METHOD(void, OnError, (ReceiverSession*, openscreen::Error error));
};

}  // namespace

TEST(ReceiverSessionTest, ThrowsErrorOnInvalidArguments) {}

TEST(ReceiverSessionTest, RegistersSelfOnMessagePump) {}

TEST(ReceiverSessionTest, CanConsumeOfferMessage) {}

}  // namespace streaming
}  // namespace cast
