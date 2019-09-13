// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_SENDER_CHANNEL_CAST_FRAMER_H_
#define CAST_SENDER_CHANNEL_CAST_FRAMER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "absl/types/span.h"
#include "platform/base/error.h"

namespace cast {
namespace channel {

class CastMessage;

using openscreen::ErrorOr;

// Class for constructing and parsing CastMessage packet data.
class MessageFramer {
 public:
  // Serializes |message_proto| into |message_data|.
  // Returns true if the message was serialized successfully, false otherwise.
  static bool Serialize(const CastMessage& message, std::string* out);

  explicit MessageFramer(absl::Span<uint8_t> input_buffer);
  ~MessageFramer();

  // The number of bytes required from the next |input_buffer| passed to Ingest
  // to complete the CastMessage being read.  Returns zero if there has been a
  // parsing error.
  size_t BytesRequested();

  // Reads bytes from |input_buffer_| and returns a new CastMessage if one is
  // fully read.
  //
  // |byte_count|     Number of additional bytes available in |input_buffer_|.
  // Returns a pointer to a parsed CastMessage if a message was received in its
  // entirety, empty unique_ptr if parsing was successful but didn't produce a
  // complete message, and an error otherwise.
  ErrorOr<std::unique_ptr<CastMessage>> Ingest(size_t byte_count);

 private:
  // Total size of the message received so far in bytes (head + body).
  size_t message_bytes_received_ = 0;

  // Data buffer wherein the caller should place message data for ingest.
  absl::Span<uint8_t> input_buffer_;

  OSP_DISALLOW_COPY_AND_ASSIGN(MessageFramer);
};

}  // namespace channel
}  // namespace cast

#endif  // CAST_SENDER_CHANNEL_CAST_FRAMER_H_
