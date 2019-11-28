// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_publisher.h"

#include "discovery/mdns/mdns_sender.h"
#include "platform/api/task_runner.h"

namespace openscreen {
namespace discovery {

MdnsPublisher::MdnsPublisher(MdnsQuerier* querier,
                             MdnsSender* sender,
                             platform::TaskRunner* task_runner,
                             MdnsRandom* random_delay)
    : querier_(querier),
      sender_(sender),
      task_runner_(task_runner),
      random_delay_(random_delay) {
  OSP_DCHECK(querier_);
  OSP_DCHECK(sender_);
  OSP_DCHECK(task_runner_);
  OSP_DCHECK(random_delay_);
}

MdnsPublisher::~MdnsPublisher() = default;

Error MdnsPublisher::RegisterRecord(const MdnsRecord& record) {
  // TODO(rwkeane): Implement this method.
  return Error::None();
}

Error MdnsPublisher::UpdateRegisteredRecord(const MdnsRecord& old_record,
                                            const MdnsRecord& new_record) {
  // TODO(rwkeane): Implement this method.
  return Error::None();
}

Error MdnsPublisher::UnregisterRecord(const MdnsRecord& record) {
  // TODO(rwkeane): Implement this method.
  return Error::None();
}

void MdnsPublisher::ClaimExclusiveOwnership(
    DomainName target_name,
    std::vector<std::function<MdnsRecord(const DomainName&)>> record_factories,
    std::function<void(DomainName)> callback) {
  ProbeInfo info;
  info.probe = std::make_unique<MdnsProbe>(querier_, sender_, task_runner_,
                                           random_delay_, this);
  info.factories = record_factories;
  info.callback = callback;

  MdnsProbe* probe = info.probe.get();
  active_probes_.push_back(std::move(info));
  probe->Start(std::move(target_name), record_factories);
}

bool MdnsPublisher::IsExclusiveOwner(const DomainName& name) {
  // TODO(rwkeane): Implement this method.
  return false;
}

bool MdnsPublisher::HasRecords(const DomainName& name,
                               DnsType type,
                               DnsClass clazz) {
  // TODO(rwkeane): Implement this method.
  return false;
}
std::vector<MdnsRecord::ConstRef> MdnsPublisher::GetRecords(
    const DomainName& name,
    DnsType type,
    DnsClass clazz) {
  // TODO(rwkeane): Implement this method.
  return std::vector<MdnsRecord::ConstRef>();
}

void MdnsPublisher::OnDomainFound(MdnsProbe* probe, DomainName name) {
  auto it = std::find_if(
      active_probes_.begin(), active_probes_.end(),
      [probe](const ProbeInfo& info) { return info.probe.get() == probe; });

  if (it == active_probes_.end()) {
    // This means that the probe we got a call from wasn't an active probe. This
    // should never happen.
    return;
  }

  // TODO(rwkeane): Mark name as claimed.

  for (auto factory : it->factories) {
    RegisterRecord(factory(name));
  }

  auto callback = it->callback;
  active_probes_.erase(it);

  if (callback) {
    callback(name);
  }
}

}  // namespace discovery
}  // namespace openscreen
