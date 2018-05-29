// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_IP_ADDRESS_H_
#define BASE_IP_ADDRESS_H_

#include <array>
#include <cstdint>

#include "base/string_span.h"

namespace openscreen {

struct IPV4Address {
 public:
  static bool Parse(cstring_span<> s, IPV4Address* address);
  // TODO: Add constructor, parser, etc.
  std::array<uint8_t, 4> bytes;
};

struct IPV6Address {
 public:
  // TODO: Add constructor, parser, etc.
  std::array<uint8_t, 16> bytes;
};

struct IPV4Endpoint {
 public:
  IPV4Address address;
  uint16_t port;
};

struct IPV6Endpoint {
 public:
  IPV6Address address;
  uint16_t port;
};

}  // namespace openscreen

#endif
