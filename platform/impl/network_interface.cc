// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/network_interface.h"

namespace openscreen {
namespace {

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

}  // namespace

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
