// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_querier.h"

#include <unordered_set>

#include "absl/types/optional.h"
#include "platform/api/task_runner.h"
#include "platform/api/time.h"
#include "util/alarm.h"

namespace cast {
namespace mdns {

using Clock = openscreen::platform::Clock;
using ClockNowFunctionPtr = openscreen::platform::ClockNowFunctionPtr;
using Alarm = openscreen::Alarm;

MdnsQuerier::MdnsQuerier(MdnsSender* sender,
                         MdnsReceiver* receiver,
                         TaskRunner* task_runner,
                         ClockNowFunctionPtr now_function,
                         MdnsRandom* random_delay)
    : sender_(sender),
      receiver_(receiver),
      task_runner_(task_runner),
      now_function_(now_function),
      random_delay_(random_delay) {
  OSP_DCHECK(sender_ != nullptr);
  OSP_DCHECK(receiver_ != nullptr);
  OSP_DCHECK(task_runner_ != nullptr);
  OSP_DCHECK(now_function_ != nullptr);
  OSP_DCHECK(random_delay_ != nullptr);

  receiver_->SetResponseCallback(
      [this](const MdnsMessage& message) { OnMessageReceived(message); });
}

MdnsQuerier::~MdnsQuerier() {
  receiver_->SetResponseCallback(nullptr);
}

void MdnsQuerier::StartQuery(const DomainName& name,
                             DnsType dns_type,
                             DnsClass dns_class,
                             MdnsRecordChangedCallback* callback) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  OSP_DCHECK(callback != nullptr);

  std::tuple<DomainName, DnsType, DnsClass> key(name, dns_type, dns_class);
  auto find_result = queries_.find(key);
  if (find_result == queries_.end()) {
    auto emplace_result = queries_.emplace(
        key, std::make_unique<MdnsQuestionTracker>(sender_, task_runner_,
                                                   Clock::now, random_delay_));

    // Since the object with this key was not found, emplace call should
    // always succeed
    OSP_DCHECK(emplace_result.second);

    find_result = emplace_result.first;
    find_result->second->Start(
        MdnsQuestion(name, dns_type, dns_class, ResponseType::kMulticast));
  }
  MdnsQuestionTracker* tracker = find_result->second.get();
  tracker->AddCallback(callback);

  // The tracker should be started after creation
  OSP_DCHECK(tracker->IsStarted());
}

void MdnsQuerier::StopQuery(const DomainName& name,
                            DnsType dns_type,
                            DnsClass dns_class,
                            MdnsRecordChangedCallback* callback) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  OSP_DCHECK(callback != nullptr);

  std::tuple<DomainName, DnsType, DnsClass> key(name, dns_type, dns_class);
  auto find_result = queries_.find(key);
  if (find_result != queries_.end()) {
    MdnsQuestionTracker* query = find_result->second.get();
    query->RemoveCallback(callback);
    if (query->CallbackCount() == 0) {
      queries_.erase(find_result);
    }
  }
}

void MdnsQuerier::OnMessageReceived(const MdnsMessage& message) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  OSP_DCHECK(message.type() == MessageType::Response);

  // TODO(yakimakha): Do we need to check authority and additional records?
  for (const MdnsRecord& record : message.answers()) {
    // TODO(yakimakha): Handle questions with type ANY and class ANY
    std::tuple<DomainName, DnsType, DnsClass> key =
        std::make_tuple(record.name(), record.dns_type(), record.dns_class());
    auto find_result = queries_.find(key);
    if (find_result != queries_.end()) {
      MdnsQuestionTracker* query = find_result->second.get();
      query->OnRecordReceived(record);
    }
  }
}

}  // namespace mdns
}  // namespace cast
