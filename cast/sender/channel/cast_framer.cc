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

// static
bool MessageFramer::Serialize(const CastMessage& message, std::string* out) {
  size_t message_size = message.ByteSizeLong();
  if (message_size > max_body_size() || message_size == 0) {
    out->clear();
    return false;
  }
  out->resize(message_size + header_size(), 0);
  openscreen::WriteBigEndian<uint32_t>(message_size, &(*out)[0]);
  if (!message.SerializeToArray(&(*out)[header_size()], message_size)) {
    out->clear();
    return false;
  }
  return true;
}

size_t MessageFramer::BytesRequested() {
  size_t bytes_left = 0;

  switch (current_element_) {
    case MessageElement::kHeader:
      bytes_left = (message_bytes_received_ < header_size())
                       ? header_size() - message_bytes_received_
                       : 0;
      OSP_DVLOG << "Bytes needed for header: " << bytes_left;
      break;
    case MessageElement::kBody:
      bytes_left = (body_size_ + header_size()) - message_bytes_received_;
      OSP_DCHECK_LE(bytes_left, max_body_size());
      OSP_DVLOG << "Bytes needed for body: " << bytes_left;
      break;
    case MessageElement::kError:
      break;
    default:
      OSP_NOTREACHED() << "Unhandled packet element type.";
      break;
  }
  return bytes_left;
}

ErrorOr<std::unique_ptr<CastMessage>> MessageFramer::Ingest(size_t byte_count) {
  if (current_element_ == MessageElement::kError) {
    return ChannelError::kCastV2InvalidMessage;
  }

  message_bytes_received_ += byte_count;
  if (message_bytes_received_ > input_buffer_.size()) {
    current_element_ = MessageElement::kError;
    return ChannelError::kCastV2InvalidMessage;
  }

  return ParseElement();
}

ErrorOr<std::unique_ptr<CastMessage>> MessageFramer::ParseElement() {
  switch (current_element_) {
    case MessageElement::kHeader:
      if (BytesRequested() == 0) {
        uint32_t message_size =
            openscreen::ReadBigEndian<uint32_t>(input_buffer_.data());
        if (message_size > max_body_size()) {
          OSP_DVLOG << "Error parsing header (message size too large).";
          current_element_ = MessageElement::kError;
          return ChannelError::kCastV2InvalidMessage;
        }
        current_element_ = MessageElement::kBody;
        body_size_ = message_size;
        if (BytesRequested() == 0) {
          return ParseElement();
        }
      }
      break;
    case MessageElement::kBody:
      if (BytesRequested() == 0) {
        auto parsed_message = std::make_unique<CastMessage>();
        if (!parsed_message->ParseFromArray(
                input_buffer_.data() + header_size(), body_size_)) {
          OSP_DVLOG << "Error parsing packet body.";
          current_element_ = MessageElement::kError;
          return ChannelError::kCastV2InvalidMessage;
        }
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
  current_element_ = MessageElement::kHeader;
  message_bytes_received_ = 0;
  body_size_ = 0;
}

}  // namespace channel
}  // namespace cast
