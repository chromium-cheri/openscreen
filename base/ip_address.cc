// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ip_address.h"

namespace openscreen {

// static
bool IPV4Address::Parse(const std::string& s, IPV4Address* address) {
  uint16_t next_octet = 0;
  int i = 0;
  bool previous_dot = false;
  for (auto c : s) {
    if (c == '.') {
      if (previous_dot) {
        return false;
      }
      address->bytes[i++] = static_cast<uint8_t>(next_octet);
      next_octet = 0;
      previous_dot = true;
      if (i > 3) {
        return false;
      }
      continue;
    }
    previous_dot = false;
    if (!std::isdigit(c)) {
      return false;
    }
    next_octet = next_octet * 10 + (c - '0');
    if (next_octet > 255) {
      return false;
    }
  }
  if (i != 3) {
    return false;
  }
  address->bytes[i] = static_cast<uint8_t>(next_octet);
  return true;
}

IPV4Address::IPV4Address() = default;
IPV4Address::IPV4Address(const std::array<uint8_t, 4>& bytes) : bytes(bytes) {}
IPV4Address::IPV4Address(const uint8_t (&b)[4])
    : bytes({b[0], b[1], b[2], b[3]}) {}
IPV4Address::IPV4Address(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4)
    : bytes({b1, b2, b3, b4}) {}
IPV4Address::IPV4Address(const IPV4Address& o) = default;

IPV4Address& IPV4Address::operator=(const IPV4Address& o) = default;

bool IPV4Address::operator==(const IPV4Address& o) const {
  return bytes == o.bytes;
}

bool IPV4Address::operator!=(const IPV4Address& o) const {
  return !(*this == o);
}

// static
bool IPV6Address::Parse(const std::string& s, IPV6Address* address) {
  if (s.size() > 1 && s[0] == ':' && s[1] != ':') {
    return false;
  }
  uint16_t next_value = 0;
  uint8_t values[16];
  int i = 0;
  int num_previous_colons = 0;
  int double_colon_index = -1;
  for (auto c : s) {
    if (c == ':') {
      ++num_previous_colons;
      if (num_previous_colons == 2) {
        double_colon_index = i;
        values[i++] = 0;
        values[i++] = 0;
        continue;
      }
      values[i++] = static_cast<uint8_t>(next_value >> 8);
      values[i++] = static_cast<uint8_t>(next_value & 0xff);
      next_value = 0;
      continue;
    }
    num_previous_colons = 0;
    uint8_t x = 0;
    if (c >= '0' && c <= '9') {
      x = c - '0';
    } else if (c >= 'a' && c <= 'f') {
      x = c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
      x = c - 'A' + 10;
    } else {
      return false;
    }
    const uint32_t candidate = next_value * 16 + x;
    if (candidate > 0xffff) {
      return false;
    } else {
      next_value = static_cast<uint16_t>(candidate);
    }
  }
  if (num_previous_colons == 1) {
    return false;
  }
  values[i++] = static_cast<uint8_t>(next_value >> 8);
  values[i++] = static_cast<uint8_t>(next_value & 0xff);
  if (double_colon_index != -1 && i != 16) {
    int j = 15;
    while (i > double_colon_index + 1) {
      values[j--] = values[--i];
    }
    for (int k = double_colon_index + 2; k <= j; ++k) {
      values[k] = 0;
    }
  }
  for (int j = 0; j < 16; ++j) {
    address->bytes[j] = values[j];
  }
  return true;
}

IPV6Address::IPV6Address() = default;
IPV6Address::IPV6Address(const std::array<uint8_t, 16>& bytes) : bytes(bytes) {}
IPV6Address::IPV6Address(const uint8_t (&b)[16])
    : bytes({b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10],
             b[11], b[12], b[13], b[14], b[15]}) {}
IPV6Address::IPV6Address(uint8_t b1,
                         uint8_t b2,
                         uint8_t b3,
                         uint8_t b4,
                         uint8_t b5,
                         uint8_t b6,
                         uint8_t b7,
                         uint8_t b8,
                         uint8_t b9,
                         uint8_t b10,
                         uint8_t b11,
                         uint8_t b12,
                         uint8_t b13,
                         uint8_t b14,
                         uint8_t b15,
                         uint8_t b16)
    : bytes({b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12, b13, b14, b15,
             b16}) {}
IPV6Address::IPV6Address(const IPV6Address& o) = default;

IPV6Address& IPV6Address::operator=(const IPV6Address& o) = default;

bool IPV6Address::operator==(const IPV6Address& o) const {
  return bytes == o.bytes;
}

bool IPV6Address::operator!=(const IPV6Address& o) const {
  return !(*this == o);
}

}  // namespace openscreen
