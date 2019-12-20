// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_DOMAIN_CLAIMED_CALLBACK_H_
#define DISCOVERY_MDNS_MDNS_DOMAIN_CLAIMED_CALLBACK_H_

#include "discovery/mdns/mdns_records.h"

namespace openscreen {
namespace discovery {

class MdnsDomainClaimedCallback {
 public:
  virtual ~MdnsDomainClaimedCallback() = default;

  // Called once the probing phase has been completed, and a DomainName has
  // been confirmed. The callee is expected to register records for the
  // newly confirmed name in this callback. Note that the requested name and
  // the confirmed name may differ if conflict resolution has occurred.
  virtual void OnDomainFound(const DomainName& requested_name,
                             const DomainName& confirmed_name) = 0;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_MDNS_DOMAIN_CLAIMED_CALLBACK_H_
