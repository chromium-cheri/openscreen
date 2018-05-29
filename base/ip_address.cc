// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ip_address.h"

namespace openscreen {

// static
bool IPV4Address::Parse(cstring_span<> s, IPV4Address* address) {
  uint8_t value = 0;
  int i = 0;
  for (auto c : s) {
    if (c == '.') {
      address->bytes[i] = value;
      value = 0;
      ++i;
      continue;
    }
    if (!std::isdigit(c)) {
      return false;
    }
    const uint16_t x = value * 10 + (c - '0');
    if (x > 255) {
      return false;
    }
    value = static_cast<uint8_t>(x);
  }
  if (i != 3) {
    return false;
  }
  address->bytes[i] = value;
  return true;
}

}  // namespace openscreen
