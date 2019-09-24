// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_MDNS_MDNS_QUERIER_H_
#define CAST_COMMON_MDNS_MDNS_QUERIER_H_

#include <random>
#include <unordered_map>

#include "absl/hash/hash.h"
#include "cast/common/mdns/mdns_random.h"
#include "cast/common/mdns/mdns_receiver.h"
#include "cast/common/mdns/mdns_sender.h"
#include "cast/common/mdns/mdns_trackers.h"
#include "platform/api/task_runner.h"

namespace cast {
namespace mdns {

using TaskRunner = openscreen::platform::TaskRunner;

class MdnsQuerier : public MdnsReceiver::Delegate {
 public:
  MdnsQuerier(MdnsSender* sender,
              MdnsReceiver* receiver,
              TaskRunner* task_runner,
              ClockNowFunctionPtr now_function,
              MdnsRandom* random_delay);
  MdnsQuerier(const MdnsQuerier& other) = delete;
  MdnsQuerier(MdnsQuerier&& other) noexcept = delete;
  ~MdnsQuerier() override;

  MdnsQuerier& operator=(const MdnsQuerier& other) = delete;
  MdnsQuerier& operator=(MdnsQuerier&& other) noexcept = delete;

  void StartQuery(const DomainName& name,
                  DnsType dns_type,
                  DnsClass record_class,
                  MdnsRecordChangedCallback* callback);

  void StopQuery(const DomainName& name,
                 DnsType dns_type,
                 DnsClass record_class,
                 MdnsRecordChangedCallback* callback);

  void OnMessageReceived(const MdnsMessage& message,
                         const IPEndpoint& sender) override;

 private:
  MdnsSender* const sender_;
  MdnsReceiver* const receiver_;
  TaskRunner* task_runner_;
  const ClockNowFunctionPtr now_function_;
  MdnsRandom* const random_delay_;

  // Storing MdnsQuestionTracker instances as unique_ptr so they are not moved
  // around in memory when other queries are added to or deleted from the
  // collection. This allows passing a pointer to MdnsQuestionTracker to a task
  // running on the TaskRunner.
  std::unordered_map<std::tuple<DomainName, DnsType, DnsClass>,
                     DelayedDeleteUniquePtr<MdnsQuestionTracker>,
                     absl::Hash<std::tuple<DomainName, DnsType, DnsClass>>>
      queries_;
  std::mutex queries_mutex_;
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_QUERIER_H_
