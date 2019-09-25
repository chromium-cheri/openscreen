// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/channel/message_framer.h"

#include <stdlib.h>
#include <string.h>

#include <limits>

#include "cast/common/channel/proto/cast_channel.pb.h"
#include "platform/api/logging.h"
#include "util/big_endian.h"
#include "util/std_util.h"

namespace cast {
namespace channel {

using ChannelError = openscreen::Error::Code;

namespace {

static constexpr size_t kHeaderSize = sizeof(uint32_t);

// Cast specifies a max message body size of 64 KiB.
static constexpr size_t kMaxBodySize = 65536;

}  // namespace

// static
ErrorOr<std::string> MessageFramer::Serialize(const CastMessage& message) {
  const size_t message_size = message.ByteSizeLong();
  if (message_size > kMaxBodySize || message_size == 0) {
    return ChannelError::kCastV2InvalidMessage;
  }
  std::string out(message_size + kHeaderSize, 0);
  openscreen::WriteBigEndian<uint32_t>(message_size, openscreen::data(out));
  if (!message.SerializeToArray(&out[kHeaderSize], message_size)) {
    return ChannelError::kCastV2InvalidMessage;
  }
  return out;
}

ErrorOr<CastMessage> MessageFramer::TryDeserialize(absl::Span<uint8_t> input,
                                                   size_t* length) {
  if (input.size() < kHeaderSize) {
    return ChannelError::kInsufficientBuffer;
  }

  const uint32_t message_size =
      openscreen::ReadBigEndian<uint32_t>(input.data());
  if (message_size > kMaxBodySize) {
    return ChannelError::kCastV2InvalidMessage;
  }

  if (input.size() < (kHeaderSize + message_size)) {
    return ChannelError::kInsufficientBuffer;
  }

  CastMessage parsed_message;
  if (!parsed_message.ParseFromArray(input.data() + kHeaderSize,
                                     message_size)) {
    return ChannelError::kCastV2InvalidMessage;
  }
  *length = kHeaderSize + message_size;

  return parsed_message;
}

}  // namespace channel
}  // namespace cast
