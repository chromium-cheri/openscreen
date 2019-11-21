// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/string.h"

#include <cmath>

#include "absl/strings/match.h"
namespace openscreen {

namespace {

// Returns byte piece of form 0b0000XXXX, or 0b11110000 on error.
uint8_t HexCharToInt(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return 0;
}

}  // namespace

Error HexToBytes(absl::string_view hex_string, uint8_t* bytes, int bytes_len) {
  if (!bytes || !bytes_len || hex_string.empty()) {
    return Error::Code::kParameterNullPointer;
  }

  // Skip leading hex prefix.
  const char kHexPrefix[] = "0x";
  if (absl::StartsWithIgnoreCase(hex_string, kHexPrefix)) {
    hex_string = hex_string.substr(strlen(kHexPrefix));
  }

  if (hex_string.size() >
      static_cast<absl::string_view::size_type>(2 * bytes_len)) {
    return Error::Code::kParameterOutOfRange;
  }

  // Left zero pad.
  int bytes_index = 0;
  const int buffer_oversize = bytes_len - ((hex_string.size() + 1) / 2);
  for (; bytes_index < buffer_oversize; ++bytes_index) {
    bytes[bytes_index] = 0;
  }

  // If the hex string contains an odd number of digits, only fill the
  // right half of the first digit.
  absl::string_view::size_type hex_index = 0;
  if (hex_string.size() % 2 == 1) {
    bytes[bytes_index] = HexCharToInt(hex_index++);
  }

  // Fill the rest of the bytes buffer normally.
  for (; bytes_index < bytes_len && hex_index < hex_string.size();
       ++bytes_index, hex_index += 2) {
    bytes[bytes_index] = (HexCharToInt(hex_string[hex_index]) << 4) +
                         HexCharToInt(hex_string[hex_index + 1]);
  }

  return Error::None();
}

}  // namespace openscreen