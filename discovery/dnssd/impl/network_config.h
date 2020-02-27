// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_IMPL_NETWORK_CONFIG_H_
#define DISCOVERY_DNSSD_IMPL_NETWORK_CONFIG_H_

#include "platform/base/interface_info.h"
#include "platform/base/ip_address.h"

namespace openscreen {
namespace discovery {

class NetworkConfig {
 public:
  ~NetworkConfig() = default;

  virtual NetworkInterfaceIndex network_interface() const = 0;

  virtual const IPAddress& address_v4() const = 0;
  virtual const IPAddress& address_v6() const = 0;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_IMPL_NETWORK_CONFIG_H_
