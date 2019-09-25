// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CHANNEL_MESSAGE_FRAMER_H_
#define CAST_COMMON_CHANNEL_MESSAGE_FRAMER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "absl/types/span.h"
#include "cast/common/channel/proto/cast_channel.pb.h"
#include "platform/base/error.h"

namespace cast {
namespace channel {

using openscreen::ErrorOr;

// Class for constructing and parsing CastMessage packet data.
class MessageFramer {
 public:
  // Serializes |message_proto| into |message_data|.
  // Returns true if the message was serialized successfully, false otherwise.
  static ErrorOr<std::string> Serialize(const CastMessage& message);

  // Reads bytes from |input| and returns a new CastMessage if one is fully
  // read.  Returns a parsed CastMessage if a message was received in its
  // entirety, and an error otherwise.  Populates |length| with the number of
  // bytes consumed from |input| when a parse succeeds.
  static ErrorOr<CastMessage> TryDeserialize(absl::Span<uint8_t> input,
                                             size_t* length);
};

}  // namespace channel
}  // namespace cast

#endif  // CAST_COMMON_CHANNEL_MESSAGE_FRAMER_H_
