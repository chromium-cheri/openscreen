// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_COMMON_CONFIG_H_
#define DISCOVERY_COMMON_CONFIG_H_

#include "platform/base/interface_info.h"

namespace openscreen {
namespace discovery {

// This struct provides parameters needed to initialize the discovery pipeline.
struct Config {
  struct NetworkInfo {
    // Network Interface on which discovery should be run.
    InterfaceInfo interface;

    // Addresses on which the service associated with this interface is running.
    bool use_ipv4;
    bool use_ipv6;
  };

  // Interfaces on which services should be published, and on which discovery
  // should listen for announced service instances.
  std::vector<NetworkInfo> network_config;

  // Number of times new mDNS records should be announced, using an exponential
  // back off. See RFC 6762 section 8.3 for further details. Per RFC, this value
  // is expected to be in the range of 2 to 8.
  int new_record_announcement_count = 8;

  // Number of times new mDNS records should be announced, using an exponential
  // back off. -1 signifies that there should be no maximum.
  int new_query_announcement_count = -1;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_COMMON_CONFIG_H_
