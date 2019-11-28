// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_PROBE_H_
#define DISCOVERY_MDNS_MDNS_PROBE_H_

#include "discovery/mdns/mdns_records.h"

namespace openscreen {

namespace platform {
class TaskRunner;
}  // namespace platform

namespace discovery {

class MdnsQuerier;
class MdnsRandom;
class MdnsSender;

// This class is responsible for claiming a domain name as described in RFC 6762
// Section 8.1's probing phase.
class MdnsProbe {
 public:
  class Observer {
   public:
    virtual ~Observer();

    // Called once the probing phase has been completed, and a DomainName has
    // been confirmed.
    virtual void OnDomainFound(MdnsProbe* probe, DomainName name) = 0;
  };

  // |querier|, |sender|, |task_runner|, |random_delay|, and |observer| must all
  // persist for the duration of this object's lifetime
  MdnsProbe(MdnsQuerier* querier,
            MdnsSender* sender,
            platform::TaskRunner* task_runner,
            MdnsRandom* random_delay,
            Observer* observer);
  ~MdnsProbe();

  // Starts probing for a valid domain name based on the given one. This may
  // only be called once per MdnsProbe instance.
  void Start(DomainName requested_name,
             std::vector<std::function<MdnsRecord(const DomainName&)>>
                 record_factories);

  // Returns whether this MdnsProbe has been started.
  bool IsRunning() const { return is_running_; }

 private:
  MdnsQuerier* const querier_;
  MdnsSender* const sender_;
  platform::TaskRunner* const task_runner_;
  MdnsRandom* const random_delay_;
  Observer* const observer_;

  bool is_running_ = false;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_MDNS_PROBE_H_
