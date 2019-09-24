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

  receiver_->SetResponseDelegate(this);
}

MdnsQuerier::~MdnsQuerier() {
  receiver_->SetResponseDelegate(nullptr);
}

void MdnsQuerier::StartQuery(const DomainName& name,
                             DnsType dns_type,
                             DnsClass record_class,
                             MdnsRecordChangedCallback* callback) {
  OSP_DCHECK(callback != nullptr);
  std::tuple<DomainName, DnsType, DnsClass> key =
      std::make_tuple(name, dns_type, record_class);
  // TODO(yakimakha): Decide when unicast response is needed.
  MdnsQuestion question(std::move(name), dns_type, record_class,
                        ResponseType::kMulticast);
  std::lock_guard<std::mutex> lock(queries_mutex_);
  auto find_result = queries_.find(key);
  if (find_result == queries_.end()) {
    auto emplace_result = queries_.emplace(
        key, MdnsQuestionTracker::Create(sender_, task_runner_, Clock::now,
                                         random_delay_));
    if (emplace_result.second) {
      emplace_result.first->second->Start(std::move(question));
    }
    // TODO(yakimakha): Do we need to check emplace_result.second here for
    // success? Return if failed to insert?

    find_result = emplace_result.first;
  }
  MdnsQuestionTracker* query = find_result->second.get();
  query->AddCallback(callback);  // TODO: notify new delegate with existing
                                 // answers if any from inside AddDelegate
}

void MdnsQuerier::StopQuery(const DomainName& name,
                            DnsType dns_type,
                            DnsClass record_class,
                            MdnsRecordChangedCallback* callback) {
  OSP_DCHECK(callback != nullptr);
  std::tuple<DomainName, DnsType, DnsClass> key =
      std::make_tuple(name, dns_type, record_class);
  std::lock_guard<std::mutex> lock(queries_mutex_);
  // Look up all entries with the matching query key, check if there's an entry
  // for the provided delegate and remove this entry if it is exists.
  auto find_result = queries_.find(key);
  if (find_result != queries_.end()) {
    MdnsQuestionTracker* query = find_result->second.get();
    query->RemoveCallback(callback);
    // if (!query->HasDelegates()) {
    //   queries_.erase(find_result);
    // }
  }
}

void MdnsQuerier::OnMessageReceived(const MdnsMessage& message,
                                    const IPEndpoint& sender) {
  OSP_DCHECK(message.type() == MessageType::Response);

  // TODO(yakimakha): Do we need to check authority and additional records?
  for (const MdnsRecord& record : message.answers()) {
    std::tuple<DomainName, DnsType, DnsClass> key =
        std::make_tuple(record.name(), record.dns_type(), record.dns_class());
    std::lock_guard<std::mutex> lock(queries_mutex_);
    auto find_result = queries_.find(key);
    if (find_result != queries_.end()) {
      MdnsQuestionTracker* query = find_result->second.get();
      query->OnRecordReceived(record);
    }
  }
}

}  // namespace mdns
}  // namespace cast
