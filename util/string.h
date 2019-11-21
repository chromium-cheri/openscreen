// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_STRING_H_
#define UTIL_STRING_H_

#include <ostream>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "platform/base/error.h"

// Helper class containing string and string_view related functions.
// Intended to fill the gaps in the Abseil and STD libraries.
namespace openscreen {

template <typename It>
void PrettyPrintAsciiHex(std::ostream& os, It first, It last) {
  auto it = first;
  while (it != last) {
    uint8_t c = *it++;
    if (c >= ' ' && c <= '~') {
      os.put(c);
    } else {
      // Output a hex escape sequence for non-printable values.
      os.put('\\');
      os.put('x');
      char digit = (c >> 4) & 0xf;
      if (digit >= 0 && digit <= 9) {
        os.put(digit + '0');
      } else {
        os.put(digit - 10 + 'a');
      }
      digit = c & 0xf;
      if (digit >= 0 && digit <= 9) {
        os.put(digit + '0');
      } else {
        os.put(digit - 10 + 'a');
      }
    }
  }
}

Error HexToBytes(absl::string_view hex_string, uint8_t* bytes, int bytes_len);

}  // namespace openscreen

#endif  // UTIL_STRING_H_
