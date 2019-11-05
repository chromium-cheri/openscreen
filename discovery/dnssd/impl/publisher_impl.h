// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_IMPL_PUBLISHER_IMPL_H_
#define DISCOVERY_DNSSD_IMPL_PUBLISHER_IMPL_H_

#include "absl/strings/string_view.h"
#include "cast/common/mdns/public/mdns_service.h"
#include "discovery/dnssd/impl/conversion_layer.h"
#include "discovery/dnssd/public/instance_record.h"
#include "discovery/dnssd/public/publisher.h"

namespace openscreen {
namespace discovery {

class PublisherImpl : public DnsSdPublisher {
 public:
  PublisherImpl(cast::mdns::MdnsService* publisher);
  ~PublisherImpl() override;

  // DnsSdPublisher overrides.
  void Register(const DnsSdInstanceRecord& record) override;
  size_t DeregisterAll(const absl::string_view& service,
                       const absl::string_view& domain) override;

 private:
  PublisherImpl(cast::mdns::MdnsService* publisher,
                std::function<std::vector<cast::mdns::MdnsRecord>(
                    const DnsSdInstanceRecord&)> GetDnsRecordsFunction);

  std::vector<DnsSdInstanceRecord> published_records_;

  cast::mdns::MdnsService* const mdns_publisher_;

  const std::function<std::vector<cast::mdns::MdnsRecord>(
      const DnsSdInstanceRecord&)>
      dns_records_factory_;

  friend class PublisherTesting;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_IMPL_PUBLISHER_IMPL_H_
