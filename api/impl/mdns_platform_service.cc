// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/mdns_platform_service.h"

#include <cstring>

namespace openscreen {

MdnsPlatformService::BoundInterfaceIPv4::BoundInterfaceIPv4(
    const platform::InterfaceInfo& interface_info,
    const platform::IPv4Subnet& subnet,
    const platform::UdpSocketIPv4Ptr& socket)
    : interface_info(interface_info), subnet(subnet), socket(socket) {}
MdnsPlatformService::BoundInterfaceIPv4::~BoundInterfaceIPv4() = default;
MdnsPlatformService::BoundInterfaceIPv6::BoundInterfaceIPv6(
    const platform::InterfaceInfo& interface_info,
    const platform::IPv6Subnet& subnet,
    const platform::UdpSocketIPv6Ptr& socket)
    : interface_info(interface_info), subnet(subnet), socket(socket) {}
MdnsPlatformService::BoundInterfaceIPv6::~BoundInterfaceIPv6() = default;

bool operator==(const MdnsPlatformService::BoundInterfaces& i1,
                const MdnsPlatformService::BoundInterfaces& i2) {
  if (i1.v4_interfaces.size() != i2.v4_interfaces.size() ||
      i1.v6_interfaces.size() != i2.v6_interfaces.size()) {
    return false;
  }
  for (auto it1 = i1.v4_interfaces.begin(), it2 = i2.v4_interfaces.begin();
       it1 != i1.v4_interfaces.end(); ++it1, ++it2) {
    if (it1->interface_info.index != it2->interface_info.index ||
        it1->interface_info.name != it2->interface_info.name ||
        std::memcmp(it1->interface_info.hardware_address,
                    it2->interface_info.hardware_address,
                    sizeof(it1->interface_info.hardware_address)) != 0 ||
        it1->interface_info.type != it2->interface_info.type) {
      return false;
    }
    if (it1->subnet.address != it2->subnet.address ||
        it1->subnet.prefix_length != it2->subnet.prefix_length) {
      return false;
    }
    if (it1->socket != it2->socket) {
      return false;
    }
  }
  for (auto it1 = i1.v6_interfaces.begin(), it2 = i2.v6_interfaces.begin();
       it1 != i1.v6_interfaces.end(); ++it1, ++it2) {
    if (it1->interface_info.index != it2->interface_info.index ||
        it1->interface_info.name != it2->interface_info.name ||
        std::memcmp(it1->interface_info.hardware_address,
                    it2->interface_info.hardware_address,
                    sizeof(it1->interface_info.hardware_address)) != 0 ||
        it1->interface_info.type != it2->interface_info.type) {
      return false;
    }
    if (it1->subnet.address != it2->subnet.address ||
        it1->subnet.prefix_length != it2->subnet.prefix_length) {
      return false;
    }
    if (it1->socket != it2->socket) {
      return false;
    }
  }
  return true;
}

}  // namespace openscreen
