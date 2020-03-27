// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_TESTING_FAKE_NETWORK_CONFIG_H_
#define DISCOVERY_DNSSD_TESTING_FAKE_NETWORK_CONFIG_H_

#include "discovery/dnssd/impl/network_config.h"
#include "gmock/gmock.h"

namespace openscreen {
namespace discovery {

class FakeNetworkConfig : public NetworkConfig {
 public:
  void set_network_interface(NetworkInterfaceIndex idx) {
    network_index_ = idx;
  }

  void set_address_v4(IPAddress address) { address_v4_ = std::move(address); }

  void set_address_v6(IPAddress address) { address_v6_ = std::move(address); }

  // NetworkConfig overrides.
  NetworkInterfaceIndex network_interface() const override {
    return network_index_;
  }
  const IPAddress& address_v4() const override { return address_v4_; }
  const IPAddress& address_v6() const override { return address_v6_; }

 private:
  NetworkInterfaceIndex network_index_;
  IPAddress address_v4_;
  IPAddress address_v6_;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_TESTING_FAKE_NETWORK_CONFIG_H_
