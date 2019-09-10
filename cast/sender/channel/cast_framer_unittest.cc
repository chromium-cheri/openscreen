// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/sender/channel/cast_framer.h"

#include <stddef.h>

#include <algorithm>
#include <string>

#include "cast/sender/channel/proto/cast_channel.pb.h"
#include "gtest/gtest.h"
#include "util/big_endian.h"

namespace cast {
namespace channel {

using ChannelError = openscreen::Error::Code;
using IngestResult = MessageFramer::IngestResult;

class CastFramerTest : public testing::Test {
 public:
  CastFramerTest()
      : buffer_(MessageFramer::MessageHeader::max_message_size()),
        framer_(absl::Span<uint8_t>(&buffer_[0], buffer_.size())) {}

  void SetUp() override {
    cast_message_.set_protocol_version(CastMessage::CASTV2_1_0);
    cast_message_.set_source_id("source");
    cast_message_.set_destination_id("destination");
    cast_message_.set_namespace_("namespace");
    cast_message_.set_payload_type(CastMessage::STRING);
    cast_message_.set_payload_utf8("payload");
    ASSERT_TRUE(MessageFramer::Serialize(cast_message_, &cast_message_str_));
  }

  void WriteToBuffer(const std::string& data) {
    memcpy(&buffer_[0], data.data(), data.size());
  }

  static constexpr size_t header_size() {
    return MessageFramer::MessageHeader::header_size();
  }
  static constexpr size_t max_body_size() {
    return MessageFramer::MessageHeader::max_body_size();
  }
  static constexpr size_t max_message_size() {
    return MessageFramer::MessageHeader::max_message_size();
  }

 protected:
  CastMessage cast_message_;
  std::string cast_message_str_;
  std::vector<uint8_t> buffer_;
  MessageFramer framer_;
};

TEST_F(CastFramerTest, TestMessageFramerCompleteMessage) {
  size_t message_length;

  WriteToBuffer(cast_message_str_);

  // Receive 1 byte of the header, framer demands 3 more bytes.
  EXPECT_EQ(4u, framer_.BytesRequested());
  IngestResult result = framer_.Ingest(1, &message_length);
  ASSERT_TRUE(result);
  EXPECT_EQ(nullptr, result.value().get());
  EXPECT_EQ(3u, framer_.BytesRequested());

  // Ingest remaining 3, expect that the framer has moved on to requesting the
  // body contents.
  result = framer_.Ingest(3, &message_length);
  ASSERT_TRUE(result);
  EXPECT_EQ(nullptr, result.value().get());
  EXPECT_EQ(cast_message_str_.size() - header_size(), framer_.BytesRequested());

  // Remainder of packet sent over the wire.
  result = framer_.Ingest(framer_.BytesRequested(), &message_length);
  ASSERT_TRUE(result);
  std::unique_ptr<CastMessage> message = result.MoveValue();
  EXPECT_NE(nullptr, message.get());
  EXPECT_EQ(message->SerializeAsString(), cast_message_.SerializeAsString());
  EXPECT_EQ(4u, framer_.BytesRequested());
  EXPECT_EQ(message->SerializeAsString().size(), message_length);
}

TEST_F(CastFramerTest, BigEndianMessageHeader) {
  size_t message_length;

  WriteToBuffer(cast_message_str_);

  EXPECT_EQ(4u, framer_.BytesRequested());
  IngestResult result = framer_.Ingest(4, &message_length);
  ASSERT_TRUE(result);
  EXPECT_EQ(nullptr, result.value().get());

  uint32_t expected_size =
      openscreen::ReadBigEndian<uint32_t>(&cast_message_str_[0]);
  EXPECT_EQ(expected_size, framer_.BytesRequested());
}

TEST_F(CastFramerTest, TestSerializeErrorMessageTooLarge) {
  std::string serialized;
  CastMessage big_message;
  big_message.CopyFrom(cast_message_);
  std::string payload;
  payload.append(max_body_size() + 1, 'x');
  big_message.set_payload_utf8(payload);
  EXPECT_FALSE(MessageFramer::Serialize(big_message, &serialized));
}

TEST_F(CastFramerTest, TestIngestIllegalLargeMessage) {
  std::string mangled_cast_message = cast_message_str_;
  mangled_cast_message[0] = 88;
  mangled_cast_message[1] = 88;
  mangled_cast_message[2] = 88;
  mangled_cast_message[3] = 88;
  WriteToBuffer(mangled_cast_message);

  size_t bytes_ingested;
  EXPECT_EQ(4u, framer_.BytesRequested());
  IngestResult result = framer_.Ingest(4, &bytes_ingested);
  ASSERT_FALSE(result);
  EXPECT_EQ(ChannelError::kCastV2InvalidMessage, result.error().code());
  EXPECT_EQ(0u, framer_.BytesRequested());

  // Test that the parser enters a terminal error state.
  WriteToBuffer(cast_message_str_);
  EXPECT_EQ(0u, framer_.BytesRequested());
  result = framer_.Ingest(4, &bytes_ingested);
  ASSERT_FALSE(result);
  EXPECT_EQ(ChannelError::kCastV2InvalidMessage, result.error().code());
  EXPECT_EQ(0u, framer_.BytesRequested());
}

TEST_F(CastFramerTest, TestIngestIllegalLargeMessage2) {
  std::string mangled_cast_message = cast_message_str_;
  // Header indicates body size is 0x00010001 = 65537
  mangled_cast_message[0] = 0;
  mangled_cast_message[1] = 0x1;
  mangled_cast_message[2] = 0;
  mangled_cast_message[3] = 0x1;
  WriteToBuffer(mangled_cast_message);

  size_t bytes_ingested;
  EXPECT_EQ(4u, framer_.BytesRequested());
  IngestResult result = framer_.Ingest(4, &bytes_ingested);
  ASSERT_FALSE(result);
  EXPECT_EQ(ChannelError::kCastV2InvalidMessage, result.error().code());
  EXPECT_EQ(0u, framer_.BytesRequested());

  // Test that the parser enters a terminal error state.
  WriteToBuffer(cast_message_str_);
  EXPECT_EQ(0u, framer_.BytesRequested());
  result = framer_.Ingest(4, &bytes_ingested);
  ASSERT_FALSE(result);
  EXPECT_EQ(ChannelError::kCastV2InvalidMessage, result.error().code());
  EXPECT_EQ(0u, framer_.BytesRequested());
}

TEST_F(CastFramerTest, TestUnparsableBodyProto) {
  // Message header is OK, but the body is replaced with "x"es.
  std::string mangled_cast_message = cast_message_str_;
  for (size_t i = header_size(); i < mangled_cast_message.size(); ++i) {
    std::fill(mangled_cast_message.begin() + header_size(),
              mangled_cast_message.end(), 'x');
  }
  WriteToBuffer(mangled_cast_message);

  // Send header.
  size_t message_length;
  EXPECT_EQ(4u, framer_.BytesRequested());
  IngestResult result = framer_.Ingest(4, &message_length);
  ASSERT_TRUE(result);
  EXPECT_EQ(nullptr, result.value().get());
  EXPECT_EQ(cast_message_str_.size() - 4, framer_.BytesRequested());

  // Send body, expect an error.
  std::unique_ptr<CastMessage> message;
  result = framer_.Ingest(framer_.BytesRequested(), &message_length);
  ASSERT_FALSE(result);
  EXPECT_EQ(ChannelError::kCastV2InvalidMessage, result.error().code());
}

}  // namespace channel
}  // namespace cast
