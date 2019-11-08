// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/instance_key.h"

#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "discovery/dnssd/impl/conversion_layer.h"
#include "discovery/dnssd/impl/service_key.h"
#include "discovery/mdns/mdns_records.h"
#include "discovery/mdns/public/mdns_constants.h"

namespace openscreen {
namespace discovery {

InstanceKey::InstanceKey(const MdnsRecord& record) {
  const DomainName& names =
      !IsPtrRecord(record)
          ? record.name()
          : absl::get<PtrRecordRdata>(record.rdata()).ptr_domain();
  OSP_DCHECK(names.labels().size() >= 4);

  auto it = names.labels().begin();
  instance_id_ = *it++;
  OSP_DCHECK(IsInstanceValid(instance_id_));

  std::string service_name = *it++;
  std::string protocol = *it++;
  service_id_ = service_name.append(".").append(protocol);
  OSP_DCHECK(IsServiceValid(service_id_));

  domain_id_ = absl::StrJoin(it, names.labels().end(), ".");
  OSP_DCHECK(IsDomainValid(domain_id_));
}

InstanceKey::InstanceKey(absl::string_view instance,
                         absl::string_view service,
                         absl::string_view domain)
    : instance_id_(instance), service_id_(service), domain_id_(domain) {
  OSP_DCHECK(IsInstanceValid(instance_id_));
  OSP_DCHECK(IsServiceValid(service_id_));
  OSP_DCHECK(IsDomainValid(domain_id_));
}

InstanceKey::InstanceKey(const InstanceKey& other) = default;
InstanceKey::InstanceKey(InstanceKey&& other) = default;

InstanceKey& InstanceKey::operator=(const InstanceKey& rhs) = default;
InstanceKey& InstanceKey::operator=(InstanceKey&& rhs) = default;

bool InstanceKey::IsInstanceOf(const ServiceKey& service_key) {
  return service_id_ == service_key.service_id() &&
         domain_id_ == service_key.domain_id();
}

}  // namespace discovery
}  // namespace openscreen
