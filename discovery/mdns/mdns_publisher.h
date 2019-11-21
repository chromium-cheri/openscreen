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

  MdnsPublisher(MdnsQuerier* querier,
                MdnsSender* sender,
                platform::TaskRunner* task_runner,
                MdnsRandom* random_delay);
  ~MdnsPublisher();

  // Registers a new mDNS Record for advertisement by this service. For A, AAAA,
  // SRV, and TXT records, the name must have already been claimed by the
  // ClaimExclusiveOWnership() method and for PTR records the name being pointed
  // to must have been claimed in the same fashion.
  void RegisterRecord(const MdnsRecord& record);

  // Updates the existing record with name matching the name of the new record.
  // This call is only valid for records of type TXT.
  void UpdateRegisteredRecord(const MdnsRecord& new_record);

  // Stops advertising the provided record. If no more records with the provided
  // name are bing advertised after this call's completion, then ownership of
  // the name is released.
  void DeregisterRecord(const MdnsRecord& record);

  // Claims the provided DomainName if it has not been claimed elsewhere, and
  // claiming a new name according to RFC 6762 Section 8.1's described probing
  // phase. Returns the name which was claimed.
  DomainName ClaimExclusiveOwnership(DomainName name);

  // Returns whether the provided name has been claimed by the
  // ClaimExclusiveOwnership() method.
  bool IsExclusiveOwner(const DomainName& name);

  // Returns whether this service has one or more records matching the provided
  // name, type, and class.
  bool HasRecords(const DomainName& name, DnsType type, DnsClass clazz);

  // Returns all records owned by this service with name, type, and class
  // matching the provided values.
  std::vector<MdnsRecordRef> GetRecords(const DomainName& name,
                                        DnsType type,
                                        DnsClass clazz);

  OSP_DISALLOW_COPY_AND_ASSIGN(MdnsPublisher);
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_MDNS_PUBLISHER_H_
