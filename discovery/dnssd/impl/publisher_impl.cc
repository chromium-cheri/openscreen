// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/publisher_impl.h"

namespace openscreen {
namespace discovery {

PublisherImpl::PublisherImpl(cast::mdns::MdnsService* publisher)
    : PublisherImpl(publisher, GetDnsRecords) {}
PublisherImpl::PublisherImpl(
    cast::mdns::MdnsService* publisher,
    std::function<std::vector<cast::mdns::MdnsRecord>(
        const DnsSdInstanceRecord&)> dns_records_factory)
    : mdns_publisher_(publisher), dns_records_factory_(dns_records_factory) {}
PublisherImpl::~PublisherImpl() = default;

void PublisherImpl::Register(const DnsSdInstanceRecord& record) {
  if (std::find(published_records_.begin(), published_records_.end(), record) !=
      published_records_.end()) {
    return;
  }

  published_records_.push_back(record);
  for (const auto& mdns_record : dns_records_factory_(record)) {
    mdns_publisher_->RegisterRecord(mdns_record);
  }
}

size_t PublisherImpl::DeregisterAll(const absl::string_view& service,
                                    const absl::string_view& domain) {
  size_t removed_count{0};
  for (auto it = published_records_.begin(); it != published_records_.end();) {
    if (it->service_id() == service && it->domain_id() == domain) {
      for (const auto& mdns_record : dns_records_factory_(*it)) {
        mdns_publisher_->DeregisterRecord(mdns_record);
      }
      removed_count++;
      it = published_records_.erase(it);
    } else {
      it++;
    }
  }

  return removed_count;
}

}  // namespace discovery
}  // namespace openscreen
