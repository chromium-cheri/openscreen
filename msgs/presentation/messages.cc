// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "msgs/presentation/messages.h"

#include <cstring>

#include "third_party/tinycbor/src/src/cbor.h"
#include "third_party/tinycbor/src/src/utf8_p.h"

namespace openscreen {
namespace msgs {
namespace presentation {
namespace {

constexpr char kRequestIdKey[] = "request-id";
constexpr char kUrlsKey[] = "urls";

#define CBOR_RETURN_WHAT_ON_ERROR(stmt, what)                    \
  {                                                              \
    CborError error = stmt;                                      \
    if (error != CborNoError && error != CborErrorOutOfMemory) { \
      return what;                                               \
    }                                                            \
  }
#define CBOR_RETURN_ON_ERROR(stmt) CBOR_RETURN_WHAT_ON_ERROR(stmt, -1)

bool IsValidUtf8(const std::string& s) {
  const uint8_t* buffer = reinterpret_cast<const uint8_t*>(s.data());
  const uint8_t* end = buffer + s.size();
  while (buffer < end) {
    // TODO(btolsch): This is an implementation detail of tinycbor so we should
    // eventually replace this call with our own utf8 validation.
    uint32_t unicode = get_utf8(&buffer, end);
    if (unicode == ~0u) {
      return false;
    }
  }
  return true;
}

bool ExpectKey(CborValue* it, const char* key, size_t key_length) {
  size_t observed_length = 0;
  CBOR_RETURN_WHAT_ON_ERROR(cbor_value_get_string_length(it, &observed_length),
                            false);
  if (observed_length != key_length) {
    return false;
  }
  std::string observed_key(key_length, 0);
  CBOR_RETURN_WHAT_ON_ERROR(
      cbor_value_copy_text_string(it, const_cast<char*>(observed_key.data()),
                                  &observed_length, nullptr),
      false);
  if (observed_key != key) {
    return false;
  }
  CBOR_RETURN_WHAT_ON_ERROR(cbor_value_advance(it), false);
  return true;
}
#define EXPECT_KEY_CONSTANT(it, key) ExpectKey(it, key, sizeof(key) - 1)

}  // namespace

ssize_t EncodeUrlAvailabilityRequest(const UrlAvailabilityRequest& request,
                                     uint8_t* buffer,
                                     size_t length) {
  CborEncoder encoder;
  CborEncoder map;
  cbor_encoder_init(&encoder, buffer, length, 0);
  CBOR_RETURN_ON_ERROR(cbor_encode_tag(
      &encoder, static_cast<uint64_t>(Tag::kUrlAvailabilityRequest)));
  CBOR_RETURN_ON_ERROR(cbor_encoder_create_map(&encoder, &map, 2));

  CBOR_RETURN_ON_ERROR(
      cbor_encode_text_string(&map, kRequestIdKey, sizeof(kRequestIdKey) - 1));
  CBOR_RETURN_ON_ERROR(cbor_encode_uint(&map, request.request_id));

  CborEncoder array;
  CBOR_RETURN_ON_ERROR(
      cbor_encode_text_string(&map, kUrlsKey, sizeof(kUrlsKey) - 1));
  CBOR_RETURN_ON_ERROR(
      cbor_encoder_create_array(&map, &array, request.urls.size()));
  for (const auto& url : request.urls) {
    if (!IsValidUtf8(url)) {
      return -1;
    }
    CBOR_RETURN_ON_ERROR(
        cbor_encode_text_string(&array, url.c_str(), url.size()));
  }
  CBOR_RETURN_ON_ERROR(cbor_encoder_close_container(&map, &array));

  CBOR_RETURN_ON_ERROR(cbor_encoder_close_container(&encoder, &map));

  size_t extra_bytes_needed = cbor_encoder_get_extra_bytes_needed(&encoder);
  if (extra_bytes_needed) {
    return static_cast<ssize_t>(length + extra_bytes_needed);
  } else {
    return static_cast<ssize_t>(cbor_encoder_get_buffer_size(&encoder, buffer));
  }
}

ssize_t DecodeUrlAvailabilityRequest(uint8_t* buffer,
                                     size_t length,
                                     UrlAvailabilityRequest* request) {
  CborParser parser;
  CborValue it;

  CBOR_RETURN_ON_ERROR(cbor_parser_init(buffer, length, 0, &parser, &it));
  // TODO(btolsch): In the future, we will read the tag first and switch on it
  // to choose a parsing function.
  if (cbor_value_get_type(&it) != CborTagType) {
    return -1;
  }
  uint64_t tag = 0;
  cbor_value_get_tag(&it, &tag);
  if (tag != static_cast<uint64_t>(Tag::kUrlAvailabilityRequest)) {
    return -1;
  }
  CBOR_RETURN_ON_ERROR(cbor_value_advance_fixed(&it));
  if (cbor_value_get_type(&it) != CborMapType) {
    return -1;
  }

  CborValue map;
  size_t map_length = 0;
  CBOR_RETURN_ON_ERROR(cbor_value_get_map_length(&it, &map_length));
  if (map_length != 2) {
    return -1;
  }
  CBOR_RETURN_ON_ERROR(cbor_value_enter_container(&it, &map));
  if (!EXPECT_KEY_CONSTANT(&map, kRequestIdKey)) {
    return -1;
  }
  CBOR_RETURN_ON_ERROR(cbor_value_get_uint64(&map, &request->request_id));
  CBOR_RETURN_ON_ERROR(cbor_value_advance_fixed(&map));

  if (!EXPECT_KEY_CONSTANT(&map, kUrlsKey)) {
    return -1;
  }

  CborValue array;
  size_t array_length = 0;
  CBOR_RETURN_ON_ERROR(cbor_value_get_array_length(&map, &array_length));
  CBOR_RETURN_ON_ERROR(cbor_value_enter_container(&map, &array));
  request->urls.reserve(array_length);
  for (size_t i = 0; i < array_length; ++i) {
    size_t url_length = 0;
    CBOR_RETURN_ON_ERROR(cbor_value_validate(&array, CborValidateUtf8));
    if (cbor_value_is_length_known(&array)) {
      CBOR_RETURN_ON_ERROR(cbor_value_get_string_length(&array, &url_length));
    } else {
      CBOR_RETURN_ON_ERROR(
          cbor_value_calculate_string_length(&array, &url_length));
    }
    request->urls.emplace_back(url_length, 0);
    CBOR_RETURN_ON_ERROR(cbor_value_copy_text_string(
        &array, const_cast<char*>(request->urls.back().data()), &url_length,
        nullptr));
    CBOR_RETURN_ON_ERROR(cbor_value_advance(&array));
  }
  CBOR_RETURN_ON_ERROR(cbor_value_leave_container(&map, &array));
  CBOR_RETURN_ON_ERROR(cbor_value_leave_container(&it, &map));

  auto result = static_cast<ssize_t>(cbor_value_get_next_byte(&it) - buffer);
  return result;
}

}  // namespace presentation
}  // namespace msgs
}  // namespace openscreen
