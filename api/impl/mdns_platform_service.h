// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_MDNS_PLATFORM_SERVICE_H_
#define API_IMPL_MDNS_PLATFORM_SERVICE_H_

#include <memory>
#include <vector>

#include "base/make_unique.h"
#include "platform/api/event_waiter.h"
#include "platform/api/network_interface.h"
#include "platform/api/socket.h"
#include "platform/base/event_loop.h"

namespace openscreen {

class MdnsPlatformService {
 public:
  struct BoundInterfaceIPv4 {
    BoundInterfaceIPv4(const platform::InterfaceInfo& interface_info,
                       const platform::IPv4Subnet& subnet,
                       const platform::UdpSocketIPv4Ptr& socket);
    ~BoundInterfaceIPv4();

    platform::InterfaceInfo interface_info;
    platform::IPv4Subnet subnet;
    platform::UdpSocketIPv4Ptr socket;
  };
  struct BoundInterfaceIPv6 {
    BoundInterfaceIPv6(const platform::InterfaceInfo& interface_info,
                       const platform::IPv6Subnet& subnet,
                       const platform::UdpSocketIPv6Ptr& socket);
    ~BoundInterfaceIPv6();

    platform::InterfaceInfo interface_info;
    platform::IPv6Subnet subnet;
    platform::UdpSocketIPv6Ptr socket;
  };
  struct BoundInterfaces {
    std::vector<BoundInterfaceIPv4> v4_interfaces;
    std::vector<BoundInterfaceIPv6> v6_interfaces;
  };

  virtual ~MdnsPlatformService() = default;

  virtual BoundInterfaces RegisterInterfaces(
      const std::vector<int32_t>& interface_index_whitelist) = 0;
  virtual void DeregisterInterfaces(
      const BoundInterfaces& registered_interfaces) = 0;
};

bool operator==(const MdnsPlatformService::BoundInterfaces& i1,
                const MdnsPlatformService::BoundInterfaces& i2);

}  // namespace openscreen

#endif  // API_IMPL_MDNS_PLATFORM_SERVICE_H_
