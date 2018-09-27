// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "msgs/messages.h"

#include <cstring>

#include "platform/api/logging.h"
#include "third_party/tinycbor/src/src/cbor.h"
#include "third_party/tinycbor/src/src/utf8_p.h"

namespace openscreen {
namespace msgs {
namespace {

#define CBOR_RETURN_WHAT_ON_ERROR(stmt, what)                           \
  {                                                                     \
    CborError error = stmt;                                             \
    /* Encoder-specific errors, so it's fine to check these even in the \
     * parser.                                                          \
     */                                                                 \
    DCHECK_NE(error, CborErrorTooFewItems);                             \
    DCHECK_NE(error, CborErrorTooManyItems);                            \
    DCHECK_NE(error, CborErrorDataTooLarge);                            \
    if (error != CborNoError && error != CborErrorOutOfMemory) {        \
      return what;                                                      \
    }                                                                   \
  }
#define CBOR_RETURN_ON_ERROR_INTERNAL(stmt) \
  CBOR_RETURN_WHAT_ON_ERROR(stmt, error)
#define CBOR_RETURN_ON_ERROR(stmt) CBOR_RETURN_WHAT_ON_ERROR(stmt, -error)

#define EXPECT_KEY_CONSTANT(it, key) ExpectKey(it, key, sizeof(key) - 1)

CborError IsValidUtf8(const std::string& s) {
  const uint8_t* buffer = reinterpret_cast<const uint8_t*>(s.data());
  const uint8_t* end = buffer + s.size();
  while (buffer < end) {
    // TODO(btolsch): This is an implementation detail of tinycbor so we should
    // eventually replace this call with our own utf8 validation.
    if (get_utf8(&buffer, end) == ~0u) {
      return CborErrorInvalidUtf8TextString;
    }
  }
  return CborNoError;
}

CborError ExpectKey(CborValue* it, const char* key, size_t key_length) {
  size_t observed_length = 0;
  CBOR_RETURN_ON_ERROR_INTERNAL(
      cbor_value_get_string_length(it, &observed_length));
  if (observed_length != key_length) {
    return CborErrorImproperValue;
  }
  std::string observed_key(key_length, 0);
  CBOR_RETURN_ON_ERROR_INTERNAL(cbor_value_copy_text_string(
      it, const_cast<char*>(observed_key.data()), &observed_length, nullptr));
  if (observed_key != key) {
    return CborErrorImproperValue;
  }
  CBOR_RETURN_ON_ERROR_INTERNAL(cbor_value_advance(it));
  return CborNoError;
}

}  // namespace

#include "msgs/osp.cc"

}  // namespace msgs
}  // namespace openscreen
