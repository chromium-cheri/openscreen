// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/fake_mdns_platform_service.h"

#include <algorithm>

#include "platform/api/logging.h"

namespace openscreen {

FakeMdnsPlatformService::FakeMdnsPlatformService() = default;
FakeMdnsPlatformService::~FakeMdnsPlatformService() = default;

MdnsPlatformService::BoundInterfaces
FakeMdnsPlatformService::RegisterInterfaces(
    const std::vector<int32_t>& interface_index_whitelist) {
  CHECK(registered_interfaces_.v4_interfaces.empty() &&
        registered_interfaces_.v6_interfaces.empty());
  if (interface_index_whitelist.empty()) {
    registered_interfaces_ = interfaces_;
  } else {
    for (const auto& interface : interfaces_.v4_interfaces) {
      if (std::find(interface_index_whitelist.begin(),
                    interface_index_whitelist.end(),
                    interface.interface_info.index) !=
          interface_index_whitelist.end()) {
        registered_interfaces_.v4_interfaces.push_back(interface);
      }
    }
    for (const auto& interface : interfaces_.v6_interfaces) {
      if (std::find(interface_index_whitelist.begin(),
                    interface_index_whitelist.end(),
                    interface.interface_info.index) !=
          interface_index_whitelist.end()) {
        registered_interfaces_.v6_interfaces.push_back(interface);
      }
    }
  }
  return registered_interfaces_;
}

void FakeMdnsPlatformService::DeregisterInterfaces(
    const BoundInterfaces& interfaces) {
  for (const auto& interface : interfaces.v4_interfaces) {
    auto index = interface.interface_info.index;
    auto it = std::find_if(registered_interfaces_.v4_interfaces.begin(),
                           registered_interfaces_.v4_interfaces.end(),
                           [index](const BoundInterfaceIPv4& interface) {
                             return interface.interface_info.index == index;
                           });
    CHECK(it != registered_interfaces_.v4_interfaces.end())
        << "Must deregister a previously returned interface: " << index;
    registered_interfaces_.v4_interfaces.erase(it);
  }
  for (const auto& interface : interfaces.v6_interfaces) {
    auto index = interface.interface_info.index;
    auto it = std::find_if(registered_interfaces_.v6_interfaces.begin(),
                           registered_interfaces_.v6_interfaces.end(),
                           [index](const BoundInterfaceIPv6& interface) {
                             return interface.interface_info.index == index;
                           });
    CHECK(it != registered_interfaces_.v6_interfaces.end())
        << "Must deregister a previously returned interface: " << index;
    registered_interfaces_.v6_interfaces.erase(it);
  }
}

}  // namespace openscreen
