// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_PUBLISHER_H_
#define DISCOVERY_MDNS_MDNS_PUBLISHER_H_

#include <functional>

#include "discovery/mdns/mdns_records.h"
#include "discovery/mdns/mdns_responder.h"

namespace openscreen {
namespace platform {

class TaskRunner;

}  // namespace platform
namespace discovery {

class MdnsPublisher : public MdnsResponder::RecordHandler {
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
  Error RegisterRecord(const MdnsRecord& record);

  // Updates the existing record with name matching the name of the new record.
  Error UpdateRegisteredRecord(const MdnsRecord& old_record,
                               const MdnsRecord& new_record);

  // Stops advertising the provided record. If no more records with the provided
  // name are bing advertised after this call's completion, then ownership of
  // the name is released.
  Error DeregisterRecord(const MdnsRecord& record);

  // Claims the provided DomainName if it has not been claimed elsewhere, and
  // claiming a new name according to RFC 6762 Section 8.1's described probing
  // phase. Returns the name which was claimed.
  DomainName ClaimExclusiveOwnership(DomainName name);

  // MdnsResponder::RecordHandler overrides.
  bool IsExclusiveOwner(const DomainName& name) override;
  bool HasRecords(const DomainName& name,
                  DnsType type,
                  DnsClass clazz) override;
  std::vector<MdnsRecordRef> GetRecords(const DomainName& name,
                                        DnsType type,
                                        DnsClass clazz) override;

  OSP_DISALLOW_COPY_AND_ASSIGN(MdnsPublisher);
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_MDNS_PUBLISHER_H_
