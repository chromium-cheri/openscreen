// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/sender/channel/cast_framer.h"

#include <stdlib.h>
#include <string.h>

#include <limits>

#include "cast/sender/channel/proto/cast_channel.pb.h"
#include "platform/api/logging.h"
#include "util/big_endian.h"

namespace cast {
namespace channel {

using ChannelError = openscreen::Error::Code;

MessageFramer::MessageFramer(absl::Span<uint8_t> input_buffer)
    : input_buffer_(input_buffer) {
  Reset();
}

MessageFramer::~MessageFramer() = default;

MessageFramer::MessageHeader::MessageHeader() = default;

void MessageFramer::MessageHeader::SetMessageSize(size_t size) {
  OSP_DCHECK_LT(size,
                static_cast<size_t>(std::numeric_limits<uint32_t>::max()));
  OSP_DCHECK_GT(size, 0U);
  message_size = size;
}

void MessageFramer::MessageHeader::PrependToString(std::string* str) {
  char bytes[header_size()];
  openscreen::WriteBigEndian<uint32_t>(message_size, bytes);
  str->insert(0, bytes, sizeof(bytes));
}

void MessageFramer::MessageHeader::Deserialize(uint8_t* data,
                                               MessageHeader* header) {
  header->message_size = openscreen::ReadBigEndian<uint32_t>(data);
}

static_assert(MessageFramer::MessageHeader::header_size() ==
                  sizeof(MessageFramer::MessageHeader),
              "Were fields added to MessageHeader without updating "
              "header_size?  If not, consider updating this assert.");

// static
bool MessageFramer::Serialize(const CastMessage& message_proto,
                              std::string* message_data) {
  message_proto.SerializeToString(message_data);
  size_t message_size = message_data->size();
  if (message_size > MessageHeader::max_body_size()) {
    message_data->clear();
    return false;
  }
  MessageHeader header;
  header.SetMessageSize(message_size);
  header.PrependToString(message_data);
  return true;
}

size_t MessageFramer::BytesRequested() {
  size_t bytes_left = 0;

  switch (current_element_) {
    case kHeader:
      OSP_DCHECK_LE(message_bytes_received_, MessageHeader::header_size());
      bytes_left = MessageHeader::header_size() - message_bytes_received_;
      OSP_DCHECK_LE(bytes_left, MessageHeader::header_size());
      OSP_DVLOG << "Bytes needed for header: " << bytes_left;
      break;
    case kBody:
      bytes_left =
          (body_size_ + MessageHeader::header_size()) - message_bytes_received_;
      OSP_DCHECK_LE(bytes_left, MessageHeader::max_body_size());
      OSP_DVLOG << "Bytes needed for body: " << bytes_left;
      break;
    case kError:
      break;
    default:
      OSP_NOTREACHED() << "Unhandled packet element type.";
      break;
  }
  return bytes_left;
}

MessageFramer::IngestResult MessageFramer::Ingest(uint32_t byte_count,
                                                  size_t* message_length) {
  if (current_element_ == kError) {
    return ChannelError::kInvalidMessage;
  }

  OSP_CHECK_LE(byte_count, BytesRequested());
  message_bytes_received_ += byte_count;
  *message_length = 0;
  switch (current_element_) {
    case kHeader:
      if (BytesRequested() == 0) {
        MessageHeader header;
        MessageHeader::Deserialize(input_buffer_.data(), &header);
        if (header.message_size > MessageHeader::max_body_size()) {
          OSP_DVLOG << "Error parsing header (message size too large).";
          current_element_ = kError;
          return ChannelError::kInvalidMessage;
        }
        current_element_ = kBody;
        body_size_ = header.message_size;
      }
      break;
    case kBody:
      if (BytesRequested() == 0) {
        auto parsed_message = std::make_unique<CastMessage>();
        if (!parsed_message->ParseFromArray(
                input_buffer_.data() + MessageHeader::header_size(),
                body_size_)) {
          OSP_DVLOG << "Error parsing packet body.";
          current_element_ = kError;
          return ChannelError::kInvalidMessage;
        }
        *message_length = body_size_;
        Reset();
        return parsed_message;
      }
      break;
    default:
      OSP_NOTREACHED() << "Unhandled packet element type.";
      return ChannelError::kUnknownError;
  }

  return std::unique_ptr<CastMessage>{};
}

void MessageFramer::Reset() {
  current_element_ = kHeader;
  message_bytes_received_ = 0;
  body_size_ = 0;
}

}  // namespace channel
}  // namespace cast
