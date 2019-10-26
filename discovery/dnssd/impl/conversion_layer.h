// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_IMPL_CONVERSION_LAYER_H_
#define DISCOVERY_DNSSD_IMPL_CONVERSION_LAYER_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "cast/common/mdns/mdns_record_changed_callback.h"
#include "discovery/dnssd/impl/constants.h"
#include "discovery/dnssd/public/instance_record.h"
#include "discovery/dnssd/public/txt_record.h"
#include "platform/base/error.h"

namespace cast {
namespace mdns {

class MdnsRecord;
class TxtRecordRdata;

}  // namespace mdns
}  // namespace cast

namespace openscreen {
namespace discovery {

// Attempts to create a new TXT record from the provided set of strings,
// returning a TxtRecord on success or an error if the provided strings are
// not valid.
ErrorOr<DnsSdTxtRecord> ConvertFromDnsTxt(
    const cast::mdns::TxtRecordRdata& txt);

// Converts the DnsData associated with this key to an InstanceRecord if
// enough data has been recieved to create a valid InstanceRecord.
// Specifically, this means that the srv, txt, and either a or aaaa fields
// have been populated. In all other cases, returns an error.
ErrorOr<DnsSdInstanceRecord> ConvertFromDnsData(const InstanceKey& instance_id,
                                                const DnsData& data);

// Modifies a DnsData entity with the provided DnsRecord. The provided change
// will always be applied, and the returned result will be an error if the
// change does not make sense from our current data state, and Error::None()
// otherwise.
// NOTE: This method should never be called with a PTR record ,and will return
// an error if it is.
Error ApplyDataRecordChange(DnsData* data,
                            const cast::mdns::MdnsRecord& record,
                            cast::mdns::RecordChangedEvent event);

// Helper functions to get the Key associated with a given DNS Record.
ErrorOr<InstanceKey> GetInstanceKey(const cast::mdns::MdnsRecord& record);
ErrorOr<ServiceKey> GetServiceKey(const cast::mdns::MdnsRecord& record);

// Creats the ServiceKey associated with the provided service and instance
// names.
ServiceKey GetServiceKey(absl::string_view service, absl::string_view instance);

inline bool IsPtrRecord(const cast::mdns::MdnsRecord& record) {
  return record.dns_type() == cast::mdns::DnsType::kPTR;
}

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_IMPL_CONVERSION_LAYER_H_
