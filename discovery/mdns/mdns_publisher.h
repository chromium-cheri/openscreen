// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_PUBLISHER_H_
#define DISCOVERY_MDNS_MDNS_PUBLISHER_H_

#include "discovery/mdns/mdns_records.h"

namespace openscreen {
namespace platform {

class TaskRunner;

}  // namespace platform
namespace discovery {

class MdnsRecord;

class MdnsPublisher {
 public:
  using RecordKey = std::tuple<DomainName, DnsType>;

  MdnsPublisher();
  ~MdnsPublisher();

  void RegisterRecord(const MdnsRecord& record);

  void UpdateRegisteredRecord(const MdnsRecord& record);

  void DeregisterRecord(const MdnsRecord& record);

  bool HasRecords(const RecordKey& key);

  std::vector<MdnsRecord> GetRecords(const RecordKey& key);

  OSP_DISALLOW_COPY_AND_ASSIGN(MdnsPublisher);
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_MDNS_PUBLISHER_H_
