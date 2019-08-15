// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_querier.h"

namespace cast {
namespace mdns {

MdnsQuerier::MdnsQuery::MdnsQuery() {}

MdnsQuerier::MdnsQuery::MdnsQuery(DomainName name, DnsType type)
    : query_(CreateMessageId(), MessageType::Query) {
  // TODO(yakimakha): Decide when to request unicast response.
  query_.AddQuestion(
      MdnsQuestion(name, type, DnsClass::kIN, false /*unicast reponse*/));
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

void MdnsQuerier::MdnsQuery::NotifyDelegates(const MdnsRecord& record) {
  for (Delegate* delegate : delegates_) {
    delegate->OnRecordReceived(record);
  }
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

  queries_lock_.lock();
  // emplace returns a pair of an iterator to the inserted or the previously
  // existing element (first) and a bool denoting whether the insertion took
  // place (second). The iterator points to a pair of key (first) and value
  // (second).
  auto emplace_result =
      queries_.emplace(std::make_pair(name, type), MdnsQuery(name, type)).first;
  MdnsQuery& query = emplace_result->second;
  query.AddDelegate(delegate);
  queries_lock_.unlock();

  // TODO(yakimakha): start sending from here or within query?

  // Query has to be sent after a random delay of 20-120 milliseconds.
  // std::chrono::milliseconds delay{query_delay_distribution_(random_engine_)};

  // TODO(yakimakha): Switch to a cancelable task when available
  // task_runner_->PostTaskWithDelay(
  //    [this, message = std::move(message)] { sender_->SendMulticast(message);
  //    }, delay);

  // TODO(yakimakha): Setup query re-send
  // TODO(yakimakha): Schedule re-query? Should cache do that?
}

void MdnsQuerier::StopQuery(const DomainName& name,
                            DnsType type,
                            Delegate* delegate) {
  OSP_DCHECK(delegate != nullptr);

  queries_lock_.lock();
  // Look up all entries with the matching query key, check if there's an entry
  // for the provided delegate and remove this entry if it is exists.
  auto find_result = queries_.find(std::make_pair(name, type));
  if (find_result != queries_.end()) {
    MdnsQuery& query = find_result->second;
    query.RemoveDelegate(delegate);
    if (query.IsEmpty()) {
      queries_.erase(find_result);
    }
  }
  queries_lock_.unlock();
}

void MdnsQuerier::OnMessageReceived(const MdnsMessage& message,
                                    const IPEndpoint& sender) {
  OSP_DCHECK(message.type() == MessageType::Response);
  for (const MdnsRecord& record : message.answers()) {
    queries_lock_.lock();
    auto find_result =
        queries_.find(std::make_pair(record.name(), record.type()));
    if (find_result != queries_.end()) {
      MdnsQuery& query = find_result->second;
      query.NotifyDelegates(record);
    }
    queries_lock_.unlock();
  }
}

}  // namespace mdns
}  // namespace cast
