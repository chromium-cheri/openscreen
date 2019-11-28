// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_probe.h"

namespace openscreen {
namespace discovery {

MdnsProbe::Observer::~Observer() = default;

MdnsProbe::MdnsProbe(MdnsQuerier* querier,
                     MdnsSender* sender,
                     platform::TaskRunner* task_runner,
                     MdnsRandom* random_delay,
                     Observer* observer)
    : querier_(querier),
      sender_(sender),
      task_runner_(task_runner),
      random_delay_(random_delay),
      observer_(observer) {
  OSP_DCHECK(querier_);
  OSP_DCHECK(sender_);
  OSP_DCHECK(task_runner_);
  OSP_DCHECK(random_delay_);
  OSP_DCHECK(observer_);
}

MdnsProbe::~MdnsProbe() = default;

void MdnsProbe::Start(DomainName requested_name,
                      std::vector<std::function<MdnsRecord(const DomainName&)>>
                          record_factories) {
  OSP_DCHECK(!is_running_);
  is_running_ = true;

  // TODO(rwkeane): Implement the probing phase.

  observer_->OnDomainFound(this, requested_name);
}

}  // namespace discovery
}  // namespace openscreen
