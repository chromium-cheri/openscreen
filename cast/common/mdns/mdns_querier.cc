// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_querier.h"

namespace cast {
namespace mdns {

MdnsQuerier::MdnsRecordTracker::MdnsRecordTracker(MdnsRecord record)
    : record_(std::move(record)) {}

void MdnsQuerier::MdnsRecordTracker::StartTracking() {}

void MdnsQuerier::MdnsRecordTracker::StopTracking() {}

MdnsQuerier::MdnsQuery::MdnsQuery(DomainName name, DnsType type)
    : query_(CreateMessageId(), MessageType::Query) {
  // TODO(yakimakha): Decide when to request unicast response.
  query_.AddQuestion(MdnsQuestion(std::move(name), type, DnsClass::kIN,
                                  false /*unicast reponse*/));

  // TODO(yakimakha): Setup query re-send
  // TODO(yakimakha): Schedule re-query? Should cache do that?
}

void MdnsQuerier::MdnsQuery::AddDelegate(Delegate* delegate) {
  auto find_result = std::find(delegates_.begin(), delegates_.end(), delegate);
  if (find_result == delegates_.end()) {
    delegates_.push_back(delegate);
  }
}

void MdnsQuerier::MdnsQuery::RemoveDelegate(Delegate* delegate) {
  auto find_result = std::find(delegates_.begin(), delegates_.end(), delegate);
  if (find_result != delegates_.end()) {
    delegates_.erase(find_result);
  }
}

bool MdnsQuerier::MdnsQuery::IsEmpty() {
  return delegates_.empty();
}

void MdnsQuerier::MdnsQuery::OnRecordReceived(const MdnsRecord& record) {
  // TODO: update TTL
  // TODO: cancel previous refreshes
  // TODO: schedule resend on TTL expiration
  // TODO: check if goodbye record
  // TODO: check if record is new or updated

  for (Delegate* delegate : delegates_) {
    delegate->OnRecordReceived(record);
  }
}

void MdnsQuerier::MdnsQuery::Send() {
  // Query has to be sent after a random delay of 20-120 milliseconds.
  // std::chrono::milliseconds delay{query_delay_distribution_(random_engine_)};
  // TODO(yakimakha): Switch to a cancelable task when available
  // task_runner_->PostTaskWithDelay(
  //    [this, message = std::move(message)] { sender_->SendMulticast(message);
  //    }, delay);
}

MdnsQuerier::MdnsQuerier(MdnsSender* sender,
                         MdnsReceiver* receiver,
                         TaskRunner* task_runner)
    : sender_(sender), receiver_(receiver), task_runner_(task_runner) {
  OSP_DCHECK(sender_ != nullptr);
  OSP_DCHECK(receiver_ != nullptr);
  OSP_DCHECK(task_runner_ != nullptr);

  receiver_->SetResponseDelegate(this);
}

void MdnsQuerier::StartQuery(const DomainName& name,
                             DnsType type,
                             Delegate* delegate) {
  OSP_DCHECK(delegate != nullptr);

  std::pair<DomainName, DnsType> key = std::make_pair(name, type);
  std::lock_guard<std::mutex> lock(queries_mutex_);
  auto find_result = queries_.find(key);
  if (find_result == queries_.end()) {
    auto emplace_result =
        queries_.emplace(key, std::make_unique<MdnsQuery>(name, type));
    // TODO(yakimakha): Do we need to check emplace_result.second here for
    // success?
    find_result = emplace_result.first;
  }
  MdnsQuery* query = find_result->second.get();
  query->AddDelegate(delegate);
}

void MdnsQuerier::StopQuery(const DomainName& name,
                            DnsType type,
                            Delegate* delegate) {
  OSP_DCHECK(delegate != nullptr);

  std::lock_guard<std::mutex> lock(queries_mutex_);
  // Look up all entries with the matching query key, check if there's an entry
  // for the provided delegate and remove this entry if it is exists.
  auto find_result = queries_.find(std::make_pair(name, type));
  if (find_result != queries_.end()) {
    MdnsQuery* query = find_result->second.get();
    query->RemoveDelegate(delegate);
    if (query->IsEmpty()) {
      queries_.erase(find_result);
    }
  }
}

void MdnsQuerier::OnMessageReceived(const MdnsMessage& message,
                                    const IPEndpoint& sender) {
  OSP_DCHECK(message.type() == MessageType::Response);

  // TODO(yakimakha): Do we need to check authority and additional records?
  for (const MdnsRecord& record : message.answers()) {
    std::lock_guard<std::mutex> lock(queries_mutex_);
    auto find_result =
        queries_.find(std::make_pair(record.name(), record.type()));
    if (find_result != queries_.end()) {
      MdnsQuery* query = find_result->second.get();
      query->OnRecordReceived(record);
    }
  }
}

}  // namespace mdns
}  // namespace cast
