// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_constants.h"

namespace libcast {
namespace mdns {

// Parsed representation: "224.0.0.251"
// clang-format off
const uint8_t kDefaultMulticastGroupIPv4[4] = {
  0xE0, 0x00, 0x00, 0xFB
};
// clang-format on

// Parsed representation: "FF02::FB"
// clang-format off
const uint8_t kDefaultMulticastGroupIpv6[16] = {
  0xFF, 0x02, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0xFB,
};
// clang-format on

// Parsed representation: "239.255.255.250"
// clang-format off
const uint8_t kDefaultSiteLocalGroupIPv4[4] = {
  0xEF, 0xFF, 0xFF, 0xFA
};
// clang-format on

// Parsed representation: "FF05::C"
// clang-format off
const uint8_t kDefaultSiteLocalGroupIPv6[16] = {
  0xFF, 0x05, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x0C,
};
// clang-format on

}  // namespace mdns
}  // namespace libcast
