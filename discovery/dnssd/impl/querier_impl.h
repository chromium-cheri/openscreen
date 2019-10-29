// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_IMPL_QUERIER_IMPL_H_
#define DISCOVERY_DNSSD_IMPL_QUERIER_IMPL_H_

#include <unordered_map>

#include "absl/hash/hash.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "cast/common/mdns/mdns_querier.h"
#include "cast/common/mdns/mdns_record_changed_callback.h"
#include "cast/common/mdns/mdns_records.h"
#include "discovery/dnssd/impl/constants.h"
#include "discovery/dnssd/impl/conversion_layer.h"
#include "discovery/dnssd/public/instance_record.h"
#include "discovery/dnssd/public/querier.h"

namespace openscreen {
namespace discovery {

class QuerierImpl : public DnsSdQuerier,
                    public cast::mdns::MdnsRecordChangedCallback {
 public:
  using MdnsQuerier = cast::mdns::MdnsQuerier;
  using MdnsRecord = cast::mdns::MdnsRecord;

  QuerierImpl(MdnsQuerier* querier);
  virtual ~QuerierImpl() override = default;

  inline bool IsQueryOngoing(const absl::string_view& service,
                             const absl::string_view& domain);

  // DnsSdQuerier overrides.
  void StartQuery(const absl::string_view& service,
                  const absl::string_view& domain,
                  Callback* cb) override;
  void StopQuery(const absl::string_view& service,
                 const absl::string_view& domain,
                 Callback* cb) override;

  // cast::mdns::MdnsRecordChangedCallback overrides.
  void OnRecordChanged(const cast::mdns::MdnsRecord& record,
                       cast::mdns::RecordChangedEvent event) override;

 private:
  // Map from {instance, service, domain} to the data recieved so far for this
  // service instance.
  std::unordered_map<InstanceKey, DnsData, absl::Hash<InstanceKey>>
      recieved_records_;

  // Stores the association between {service, domain} and Callback to call for
  // records with this ServiceKey.
  std::map<ServiceKey, std::vector<Callback*>> callback_map_;

  MdnsQuerier* querier_;

  friend class QuerierImplTesting;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_IMPL_QUERIER_IMPL_H_
