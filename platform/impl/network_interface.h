// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_NETWORK_INTERFACE_H_
#define PLATFORM_IMPL_NETWORK_INTERFACE_H_

#include <vector>

#include "absl/types/optional.h"
#include "platform/base/interface_info.h"

namespace openscreen {

// Returns all interfaces of the given types, represented by flag enum |types|.
std::vector<InterfaceInfo> GetAllInterfaces();

// Returns interfaces of a given type.
std::vector<InterfaceInfo> GetTypedInterfaces(InterfaceInfo::Type types) {
  std::vector<InterfaceInfo> interfaces = GetAllInterfaces();

  std::vector<InterfaceInfo> results;
  results.reserve(interfaces.size());
  for (auto it = interfaces.begin(); it != interfaces.end(); it++) {
    if ((it->type & types) != static_cast<InterfaceInfo::Type>(0)) {
      results.push_back(std::move(*it));
    }
  }
  return results;
}

std::vector<InterfaceInfo> GetNetworkInterfaces() {
  return GetTypedInterfaces(InterfaceInfo::Type::kEthernet |
                            InterfaceInfo::Type::kWifi |
                            InterfaceInfo::Type::kOther);
}

// Returns an InterfaceInfo associated with the system's loopback interface.
absl::optional<InterfaceInfo> GetLoopbackInterfaceForTesting() {
  std::vector<InterfaceInfo> interfaces =
      GetTypedInterfaces(InterfaceInfo::Type::kLoopback);
  auto it = std::find_if(
      interfaces.begin(), interfaces.end(), [](const InterfaceInfo& info) {
        return std::find_if(info.addresses.begin(), info.addresses.end(),
                            [](const IPSubnet& subnet) {
                              const IPAddress loopback_address{127, 0, 0, 1};
                              return subnet.address == loopback_address;
                            }) != info.addresses.end();
      });

  if (it == interfaces.end()) {
    return absl::nullopt;
  } else {
    return *it;
  }
}

}  // namespace openscreen

#endif  // PLATFORM_IMPL_NETWORK_INTERFACE_H_
