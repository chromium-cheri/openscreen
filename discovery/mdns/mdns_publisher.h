// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_PUBLISHER_H_
#define DISCOVERY_MDNS_MDNS_PUBLISHER_H_

#include <functional>

#include "discovery/mdns/mdns_records.h"

namespace openscreen {
namespace platform {

class TaskRunner;

}  // namespace platform
namespace discovery {

class MdnsPublisher {
 public:
  using MdnsRecordRef = std::reference_wrapper<const MdnsRecord>;

  MdnsPublisher();
  ~MdnsPublisher();

  void RegisterRecord(const MdnsRecord& record);

  void UpdateRegisteredRecord(const MdnsRecord& record);

  void DeregisterRecord(const MdnsRecord& record);

  bool IsExclusiveOwner(const DomainName& name);

  bool HasRecords(const DomainName& name, DnsType type, DnsClass clazz);

  std::vector<MdnsRecordRef> GetRecords(const DomainName& name,
                                        DnsType type,
                                        DnsClass clazz);

  OSP_DISALLOW_COPY_AND_ASSIGN(MdnsPublisher);
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_MDNS_PUBLISHER_H_
