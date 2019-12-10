// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_probe.h"

#include <utility>

#include "platform/api/task_runner.h"

namespace openscreen {
namespace discovery {

MdnsProbe::Observer::~Observer() = default;

MdnsProbe::MdnsProbe(MdnsSender* sender,
                     MdnsQuerier* querier,
                     MdnsRandom* random_delay,
                     TaskRunner* task_runner,
                     Observer* observer,
                     DomainName domain,
                     IPEndpoint endpoint)
    : sender_(sender),
      querier_(querier),
      random_delay_(random_delay),
      task_runner_(task_runner),
      observer_(observer),
      target_(std::move(domain)),
      endpoint_(std::move(endpoint)) {
  OSP_DCHECK(querier_);
  OSP_DCHECK(sender_);
  OSP_DCHECK(task_runner_);
  OSP_DCHECK(random_delay_);
  OSP_DCHECK(observer_);

  PerformProbeIteration();
}

MdnsProbe::~MdnsProbe() = default;

void MdnsProbe::PerformProbeIteration() {
  // TODO(rwkeane): Implement this method.

  task_runner_->PostTask([this]() { observer_->OnProbeSuccess(this); });
}

}  // namespace discovery
}  // namespace openscreen
