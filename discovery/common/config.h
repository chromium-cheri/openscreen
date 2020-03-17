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
  /*****************************************
   * Networking Settings
   *****************************************/

  // Network Interface on which mDNS should be run.
  InterfaceInfo interface;

  /*****************************************
   * Publisher Settings
   *****************************************/

  // Determines whether publishing of services is enabled.
  bool enable_publication = true;

  // Number of times new mDNS records should be announced, using an exponential
  // back off. See RFC 6762 section 8.3 for further details. Per RFC, this value
  // is expected to be in the range of 2 to 8.
  int new_record_announcement_count = 8;

  /*****************************************
   * Querier Settings
   *****************************************/

  // Determines whether querying is enabled.
  bool enable_querying = true;

  // Number of times new mDNS records should be announced, using an exponential
  // back off. -1 signifies that there should be no maximum.
  // NOTE: This is expected to be -1 in all production scenarios and only be a
  // different value during testing.
  int new_query_announcement_count = -1;

  // Limits on the size to which the mDNS Cache may grow. This is used to
  // prevent a malicious or misbehaving mDNS client from causing the memory
  // used by mDNS to grow in an unbounded fashion.
  //
  // When the |querier_max_records_per_domain| is exceeded, the least recently
  // used records will be dropped and treated as expired.
  // When the |querier_max_domains_tracked| is exceeded, new domains received
  // will not be tracked until existing domains are removed.
  //
  // NOTE: The max size of an mDNS Message is 512 bytes, so with domain
  // compression the max size of the mDNS cache querier cache can be roughly
  // bounded by
  // 1024 * |querier_max_records_per_domain| * |querier_max_domains_tracked|
  int querier_max_records_per_domain = 8;
  int querier_max_domains_tracked = 512;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_COMMON_CONFIG_H_
