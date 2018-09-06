// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "control/messages.h"

#include <cstring>

#include "third_party/tinycbor/src/src/cbor.h"

namespace openscreen {
namespace {

constexpr char kRequestIdKey[] = "request-id";
constexpr char kUrlsKey[] = "urls";

constexpr uint32_t kPresentationUrlAvailabilityRequestTag = 2000u;

}  // namespace

ssize_t EncodePresentationUrlAvailabilityRequest(
    uint64_t request_id,
    const std::vector<std::string>& urls,
    uint8_t* buffer,
    ssize_t length) {
  CborEncoder encoder;
  CborError error;
  CborEncoder map;
  cbor_encoder_init(&encoder, buffer, length, 0);
  error = cbor_encode_tag(&encoder, kPresentationUrlAvailabilityRequestTag);
  if (error != CborNoError) {
    return -1;
  }
  error = cbor_encoder_create_map(&encoder, &map, 2);
  if (error != CborNoError) {
    return -1;
  }

  error =
      cbor_encode_text_string(&map, kRequestIdKey, sizeof(kRequestIdKey) - 1);
  if (error != CborNoError) {
    return -1;
  }
  error = cbor_encode_uint(&map, request_id);
  if (error != CborNoError) {
    return -1;
  }

  CborEncoder array;
  error = cbor_encode_text_string(&map, kUrlsKey, sizeof(kUrlsKey) - 1);
  if (error != CborNoError) {
    return -1;
  }
  error = cbor_encoder_create_array(&map, &array, urls.size());
  if (error != CborNoError) {
    return -1;
  }
  for (const auto& url : urls) {
    error = cbor_encode_text_string(&array, url.c_str(), url.size());
    if (error != CborNoError) {
      return -1;
    }
  }
  error = cbor_encoder_close_container(&map, &array);
  if (error != CborNoError) {
    return -1;
  }

  error = cbor_encoder_close_container(&encoder, &map);
  if (error != CborNoError) {
    return -1;
  }

  return static_cast<ssize_t>(cbor_encoder_get_buffer_size(&encoder, buffer));
}

ssize_t DecodePresentationUrlAvailabilityRequest(
    uint8_t* buffer,
    ssize_t length,
    PresentationUrlAvailabilityRequest* request) {
  CborParser parser;
  CborValue it;
  CborError error;

  error = cbor_parser_init(buffer, length, 0, &parser, &it);
  if (error != CborNoError) {
    return -1;
  }
  // TODO(btolsch): In the future, we will read the tag first and switch on it
  // to choose a parsing function.
  if (cbor_value_get_type(&it) != CborTagType ||
      it.extra != kPresentationUrlAvailabilityRequestTag) {
    return -1;
  }
  error = cbor_value_advance_fixed(&it);
  if (error != CborNoError) {
    return -1;
  }
  if (cbor_value_get_type(&it) != CborMapType) {
    return -1;
  }

  CborValue map;
  size_t map_length = 0;
  error = cbor_value_get_map_length(&it, &map_length);
  if (error != CborNoError || map_length != 2) {
    return -1;
  }
  error = cbor_value_enter_container(&it, &map);
  if (error != CborNoError) {
    return -1;
  }
  std::string key;
  size_t key_length = 0;
  error = cbor_value_get_string_length(&map, &key_length);
  if (error != CborNoError || key_length != (sizeof(kRequestIdKey) - 1)) {
    return -1;
  }
  key.resize(key_length);
  error = cbor_value_copy_text_string(&map, const_cast<char*>(key.data()),
                                      &key_length, nullptr);
  if (error != CborNoError || key != kRequestIdKey) {
    return -1;
  }
  key_length = 0;
  key.clear();
  error = cbor_value_advance(&map);
  if (error != CborNoError) {
    return -1;
  }
  error = cbor_value_get_uint64(&map, &request->request_id);
  if (error != CborNoError) {
    return -1;
  }
  error = cbor_value_advance_fixed(&map);
  if (error != CborNoError) {
    return -1;
  }

  error = cbor_value_get_string_length(&map, &key_length);
  if (error != CborNoError || key_length != (sizeof(kUrlsKey) - 1)) {
    return -1;
  }
  key.resize(key_length);
  error = cbor_value_copy_text_string(&map, const_cast<char*>(key.data()),
                                      &key_length, nullptr);
  if (error != CborNoError || key != kUrlsKey) {
    return -1;
  }
  error = cbor_value_advance(&map);
  if (error != CborNoError) {
    return -1;
  }

  CborValue array;
  size_t array_length = 0;
  error = cbor_value_get_array_length(&map, &array_length);
  if (error != CborNoError) {
    return -1;
  }
  error = cbor_value_enter_container(&map, &array);
  if (error != CborNoError) {
    return -1;
  }
  request->urls.reserve(array_length);
  for (size_t i = 0; i < array_length; ++i) {
    std::string url;
    size_t url_length = 0;
    if (cbor_value_is_length_known(&array)) {
      error = cbor_value_get_string_length(&array, &url_length);
      if (error != CborNoError) {
        return -1;
      }
    } else {
      error = cbor_value_calculate_string_length(&array, &url_length);
      if (error != CborNoError) {
        return -1;
      }
    }
    url.resize(url_length);
    error = cbor_value_copy_text_string(&array, const_cast<char*>(url.data()),
                                        &url_length, nullptr);
    if (error != CborNoError) {
      return -1;
    }
    request->urls.push_back(std::move(url));
    error = cbor_value_advance(&array);
    if (error != CborNoError) {
      return -1;
    }
  }
  error = cbor_value_leave_container(&map, &array);
  if (error != CborNoError) {
    return -1;
  }

  error = cbor_value_leave_container(&it, &map);
  if (error != CborNoError) {
    return -1;
  }
  auto result = static_cast<ssize_t>(cbor_value_get_next_byte(&it) - buffer);
  return result;
}

}  // namespace openscreen
