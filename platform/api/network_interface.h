// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_NETWORK_INTERFACE_H_
#define PLATFORM_API_NETWORK_INTERFACE_H_

#include <vector>

#include "base/ip_address.h"

namespace openscreen {
namespace platform {

struct InterfaceInfo {
  int32_t index;
  uint8_t hardware_address[6];
  std::string name;
};

struct IPv4InterfaceAddress {
  InterfaceInfo info;
  IPv4Address address;
  uint8_t prefix_length;
};

struct IPv6InterfaceAddress {
  InterfaceInfo info;
  IPv6Address address;
  uint8_t prefix_length;
};

struct InterfaceAddresses {
  std::vector<IPv4InterfaceAddress> v4_addresses;
  std::vector<IPv6InterfaceAddress> v6_addresses;
};

InterfaceAddresses GetInterfaceAddresses();

}  // namespace platform
}  // namespace openscreen

#endif
