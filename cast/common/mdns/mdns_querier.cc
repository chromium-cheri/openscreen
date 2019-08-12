// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_querier.h"

namespace cast {
namespace mdns {

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
  delegates_lock_.lock();
  delegates_.emplace(std::make_pair(name, type), delegate);
  delegates_lock_.unlock();

  MdnsMessage message(CreateMessageId(), MessageType::Query);
  // TODO(yakimakha): Decide when to use unicast response.
  message.AddQuestion(
      MdnsQuestion(name, type, DnsClass::kIN, false /*unicast response*/));

  // Query has to be sent after a random delay of 20-120 milliseconds.
  std::chrono::milliseconds delay{query_delay_distribution_(random_engine_)};

  // TODO(yakimakha): Switch to a cancelable task when available
  task_runner_->PostTaskWithDelay(
      [this, message = std::move(message)] { sender_->SendMulticast(message); },
      delay);

  // TODO(yakimakha): Setup query re-send
}

void MdnsQuerier::StopQuery(const DomainName& name,
                            DnsType type,
                            Delegate* delegate) {
  auto key = std::make_pair(name, type);
  delegates_lock_.lock();
  // Look up all entries with the matching query key, check if there's an entry
  // for the provided delegate and remove this entry if it is exists.
  auto entries = delegates_.equal_range(key);
  for (auto entry = entries.first; entry != entries.second; ++entry) {
    if (entry->second == delegate) {
      delegates_.erase(entry);
      break;
    }
  }
  delegates_lock_.unlock();
}

void MdnsQuerier::OnMessageReceived(const MdnsMessage& message,
                                    const IPEndpoint& sender) {
  OSP_DCHECK(message.type() == MessageType::Response);
  for (const MdnsRecord& record : message.answers()) {
    auto key = std::make_pair(record.name(), record.type());
    std::vector<Delegate*> delegates;

    delegates_lock_.lock();
    // Copy all delegates with the matching query key to a local collection to
    // exit the lock as quickly as possible.
    auto entries = delegates_.equal_range(key);
    for (auto entry = entries.first; entry != entries.second; ++entry) {
      delegates.push_back(entry->second);
    }
    delegates_lock_.unlock();

    for (Delegate* delegate : delegates) {
      delegate->OnRecordReceived(record);
    }
  }
  // TODO(yakimakha): Update responses in cache
  // TODO(yakimakha): Schedule re-query? Should cache do that?
}

}  // namespace mdns
}  // namespace cast
