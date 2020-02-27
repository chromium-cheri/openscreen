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
    NetworkInterfaceIndex interface;

    // Addresses on which the service associated with this interface is running.
    IPAddress v4;
    IPAddress v6;
  };

  // Interfaces on which services should be published, and on which discovery
  // should listen for announced service instances.
  std::vector<NetworkInfo> network_config;

  // Number of times new mDNS records should be announced. See RFC 6762 section
  // 8.3 for further details. Per RFC, this value is expected to be in the range
  // of 2 to 8.
  int new_record_announcement_count = 8;

  // Determines whether new mDNS Queries should be sent out over the network.
  bool should_announce_new_queries_ = true;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_COMMON_CONFIG_H_
