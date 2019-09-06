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
#include "platform/api/task_runner.h"

namespace cast {
namespace mdns {

using TaskRunner = openscreen::platform::TaskRunner;

class MdnsQuerier : public MdnsReceiver::Delegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // TODO: Add reason: record added, record updated, record removed
    virtual void OnRecordChanged(const MdnsRecord& record) = 0;
  };

  MdnsQuerier(MdnsSender* sender,
              MdnsReceiver* receiver,
              TaskRunner* task_runner);
  MdnsQuerier(const MdnsQuerier& other) = delete;
  MdnsQuerier(MdnsQuerier&& other) noexcept = delete;
  ~MdnsQuerier() override;

  MdnsQuerier& operator=(const MdnsQuerier& other) = delete;
  MdnsQuerier& operator=(MdnsQuerier&& other) noexcept = delete;

  void StartQuery(const DomainName& name, DnsType type, Delegate* delegate);
  void StopQuery(const DomainName& name, DnsType type, Delegate* delegate);

  void OnMessageReceived(const MdnsMessage& message,
                         const IPEndpoint& sender) override;

 private:
  class MdnsRecordTracker;
  class MdnsQuestionTracker;

  MdnsSender* const sender_;
  MdnsReceiver* const receiver_;
  TaskRunner* task_runner_;

  // Storing MdnsQuestionTracker instances as unique_ptr so they are not moved
  // around in memory when other queries are added to or deleted from the
  // collection. This allows passing a pointer to MdnsQuestionTracker to a task
  // running on the TaskRunner.
  std::unordered_map<std::pair<DomainName, DnsType>,
                     std::unique_ptr<MdnsQuestionTracker>,
                     absl::Hash<std::pair<DomainName, DnsType>>>
      queries_;
  std::mutex queries_mutex_;

  std::default_random_engine random_engine_;
  std::uniform_int_distribution<int64_t> query_delay_distribution_{20, 120};
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_QUERIER_H_
