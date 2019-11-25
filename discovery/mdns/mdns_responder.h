// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_RESPONDER_H_
#define DISCOVERY_MDNS_MDNS_RESPONDER_H_

#include <functional>
#include <vector>

#include "discovery/mdns/mdns_records.h"
#include "platform/api/time.h"
#include "platform/base/macros.h"

namespace openscreen {

struct IPEndpoint;

namespace platform {

class TaskRunner;

}  // namespace platform
namespace discovery {

class MdnsMessage;
class MdnsRandom;
class MdnsReceiver;
class MdnsRecordChangedCallback;
class MdnsSender;
class MdnsQuerier;

class MdnsResponder {
 public:
  // Class to handle querying for existing records.
  class RecordHandler {
    using MdnsRecordRef = std::reference_wrapper<const MdnsRecord>;

    // Returns whether the provided name is exclusively owned by this endpoint.
    virtual bool IsExclusiveOwner(const DomainName& name) = 0;

    // Returns whether this service has one or more records matching the
    // provided name, type, and class.
    virtual bool HasRecords(const DomainName& name,
                            DnsType type,
                            DnsClass clazz) = 0;

    // Returns all records owned by this service with name, type, and class
    // matching the provided values.
    virtual std::vector<MdnsRecordRef> GetRecords(const DomainName& name,
                                                  DnsType type,
                                                  DnsClass clazz) = 0;
  };

  MdnsResponder(RecordHandler* record_handler,
                MdnsSender* sender,
                MdnsReceiver* receiver,
                MdnsQuerier* querier,
                platform::TaskRunner* task_runner,
                platform::ClockNowFunctionPtr now_function,
                MdnsRandom* random_delay);
  ~MdnsResponder();

  OSP_DISALLOW_COPY_AND_ASSIGN(MdnsResponder);

 private:
  void OnMessageReceived(const MdnsMessage& message, const IPEndpoint& src);

  RecordHandler* const record_handler_;
  MdnsSender* const sender_;
  MdnsReceiver* const receiver_;
  MdnsQuerier* const querier_;
  platform::TaskRunner* const task_runner_;
  const platform::ClockNowFunctionPtr now_function_;
  MdnsRandom* const random_delay_;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_MDNS_RESPONDER_H_
