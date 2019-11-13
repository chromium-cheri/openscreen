// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/service_key.h"

#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "discovery/dnssd/impl/conversion_layer.h"
#include "discovery/dnssd/impl/instance_key.h"
#include "discovery/mdns/mdns_records.h"
#include "discovery/mdns/public/mdns_constants.h"

namespace openscreen {
namespace discovery {

ServiceKey::ServiceKey(const MdnsRecord& record)
    : ServiceKey(InstanceKey(record)) {}

ServiceKey::ServiceKey(const InstanceKey& key)
    : ServiceKey(key.service_id(), key.domain_id()) {
  OSP_DCHECK(IsServiceValid(service_id_));
  OSP_DCHECK(IsDomainValid(domain_id_));
}

ServiceKey::ServiceKey(absl::string_view service, absl::string_view domain)
    : service_id_(service.data(), service.size()),
      domain_id_(domain.data(), domain.size()) {
  OSP_DCHECK(IsServiceValid(service_id_));
  OSP_DCHECK(IsDomainValid(domain_id_));
}

ServiceKey::ServiceKey(const ServiceKey& other) = default;
ServiceKey::ServiceKey(ServiceKey&& other) = default;

ServiceKey& ServiceKey::operator=(const ServiceKey& rhs) = default;
ServiceKey& ServiceKey::operator=(ServiceKey&& rhs) = default;

}  // namespace discovery
}  // namespace openscreen
