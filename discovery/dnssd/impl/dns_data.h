// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_IMPL_DNS_DATA_H_
#define DISCOVERY_DNSSD_IMPL_DNS_DATA_H_

#include "cast/common/mdns/mdns_record_changed_callback.h"
#include "cast/common/mdns/mdns_records.h"
#include "discovery/dnssd/impl/constants.h"
#include "discovery/dnssd/public/instance_record.h"
#include "platform/base/error.h"

namespace openscreen {
namespace discovery {

// This is the set of DNS data that can be associated with a single PTR record.
class DnsData {
 public:
  virtual ~DnsData() = default;

  // Converts this DnsData to an InstanceRecord if enough data has been
  // populated to create a valid InstanceRecord. Else, an error is returned.
  virtual ErrorOr<DnsSdInstanceRecord> CreateRecord() = 0;

  // Modifies this entity with the provided DnsRecord. If called with a valid
  // record type, the provided change will always be applied. The returned
  // result will be an error if the change does not make sense from our current
  // data state, and Error::None() otherwise. In the case of an error, no change
  // will be made. Valid record types with which this method can be called are
  // SRV, TXT, A, and AAAA record types.
  virtual Error ApplyDataRecordChange(const cast::mdns::MdnsRecord& record,
                                      cast::mdns::RecordChangedEvent event) = 0;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_IMPL_DNS_DATA_H_
