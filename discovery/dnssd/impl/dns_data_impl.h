// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_IMPL_DNS_DATA_IMPL_H_
#define DISCOVERY_DNSSD_IMPL_DNS_DATA_IMPL_H_

#include "absl/types/optional.h"
#include "cast/common/mdns/mdns_record_changed_callback.h"
#include "cast/common/mdns/mdns_records.h"
#include "discovery/dnssd/impl/constants.h"
#include "discovery/dnssd/impl/dns_data.h"
#include "discovery/dnssd/public/instance_record.h"

namespace openscreen {
namespace discovery {

// This is the set of DNS data that can be associated with a single PTR record.
class DnsDataImpl : public DnsData {
 public:
  explicit DnsDataImpl(const InstanceKey& instance_id);
  ~DnsDataImpl() override;

  // DnsData overrides.
  ErrorOr<DnsSdInstanceRecord> CreateRecord() override;
  Error ApplyDataRecordChange(const cast::mdns::MdnsRecord& record,
                              cast::mdns::RecordChangedEvent event) override;

 private:
  absl::optional<cast::mdns::SrvRecordRdata> srv_;
  absl::optional<cast::mdns::TxtRecordRdata> txt_;
  absl::optional<cast::mdns::ARecordRdata> a_;
  absl::optional<cast::mdns::AAAARecordRdata> aaaa_;

  InstanceKey instance_id_;

  friend class DnsDataTesting;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_IMPL_DNS_DATA_IMPL_H_
