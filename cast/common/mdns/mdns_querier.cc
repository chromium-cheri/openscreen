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

namespace {
// RFC 6762 Section 5.2
// https://tools.ietf.org/html/rfc6762#section-5.2
constexpr int kIntervalIncreaseFactor = 2;
constexpr std::chrono::seconds kMinimumQueryInterval{1};
constexpr std::chrono::minutes kMaximumQueryInterval{60};
}  // namespace

class MdnsQuerier::MdnsRecordTracker {
 public:
  MdnsRecordTracker(MdnsSender* sender,
                    TaskRunner* task_runner,
                    ClockNowFunctionPtr now_function)
      : now_function_(now_function),
        // sender_(sender),
        expired_alarm(now_function, task_runner),
        refresh_alarm(now_function, task_runner) {
    OSP_DCHECK(sender);
    OSP_DCHECK(task_runner);
    OSP_DCHECK(now_function);
  }

  MdnsRecordTracker(const MdnsRecordTracker& other) = delete;
  MdnsRecordTracker(MdnsRecordTracker&& other) noexcept = delete;
  ~MdnsRecordTracker() = default;

  MdnsRecordTracker& operator=(const MdnsRecordTracker& other) = delete;
  MdnsRecordTracker& operator=(MdnsRecordTracker&& other) noexcept = delete;

  void StartTracking(MdnsRecord record) {
    if (record_.has_value()) {
      // Check if we need to send record updated
    }
    record_ = std::move(record);
    last_update_time_ = now_function_();
    expired_alarm.Schedule(
        [this] { OnExpired(); },
        last_update_time_ + std::chrono::seconds(record.ttl()));
    refresh_alarm.Schedule(
        [this] { OnRefresh(); },
        last_update_time_ + std::chrono::seconds(record.ttl()));  // 0.8 ?
  }

  void StopTracking() {
    expired_alarm.Cancel();
    refresh_alarm.Cancel();
    record_.reset();
  }

  void Update(MdnsRecord record) {
    StopTracking();
    StartTracking(std::move(record));
  }

  // TODO: re-query only with the correct type, not ANY
 private:
  void OnExpired() {
    // Call record changed with record deleted
    // Schedule delayed deletion if necessary (in Querier)
  }

  void OnRefresh() {
    expired_alarm.Cancel();  // Do we need to cancel this?
    refresh_alarm.Cancel();
    // Call send refresh
    // Schedule another refresh
  }

  const ClockNowFunctionPtr now_function_;
  // MdnsSender* sender_;
  // TODO(yakimakha): Switch to a cancelable task when available
  Alarm expired_alarm;
  Alarm refresh_alarm;
  // only need type, rdata, ttl, probably don't need record type
  absl::optional<MdnsRecord> record_;
  Clock::time_point last_update_time_;
  // expiration task, marks record as expired? lets querier know the record is
  // expired? refresh task
};

class MdnsQuerier::MdnsQuestionTracker {
 public:
  MdnsQuestionTracker(MdnsSender* sender,
                      TaskRunner* task_runner,
                      ClockNowFunctionPtr now_function)
      : now_function_(now_function),
        // task_runner_(task_runner),
        sender_(sender),
        refresh_alarm(now_function, task_runner) {
    OSP_DCHECK(sender);
    OSP_DCHECK(task_runner);
    OSP_DCHECK(now_function);
  }

  MdnsQuestionTracker(const MdnsQuestionTracker& other) = delete;
  MdnsQuestionTracker(MdnsQuestionTracker&& other) noexcept = delete;
  ~MdnsQuestionTracker() = default;

  MdnsQuestionTracker& operator=(const MdnsQuestionTracker& other) = delete;
  MdnsQuestionTracker& operator=(MdnsQuestionTracker&& other) noexcept = delete;

  Error Start(MdnsQuestion question) {
    if (question_.has_value()) {
      return Error::Code::kOperationInvalid;
    }

    question_ = std::move(question);
    refresh_delay_ = kMinimumQueryInterval;
    // The initial query has to be sent after a random delay of 20-120
    // milliseconds.
    Clock::duration delay = mdns_random_->GetInitialQueryDelay();
    refresh_alarm.Schedule(std::bind(&MdnsQuestionTracker::Refresh, this),
                           now_function_() + delay);
    return Error::None();
  }

  Error Stop() {
    if (!question_.has_value()) {
      return Error::Code::kOperationInvalid;
    }

    refresh_alarm.Cancel();
    question_.reset();
    return Error::None();
  }

  // bool IsTracking() {
  //  return question_.has_value();
  //};

  void AddDelegate(Delegate* delegate) { delegates_.insert(delegate); }

  void RemoveDelegate(Delegate* delegate) { delegates_.erase(delegate); }

  bool IsEmpty() { return delegates_.empty(); }

  void OnRecordReceived(const MdnsRecord& record) {
    // TODO: update TTL
    // TODO: cancel previous refreshes
    // TODO: schedule resend on TTL expiration
    // TODO: check if goodbye record
    // TODO: check if record is new or updated

    for (Delegate* delegate : delegates_) {
      delegate->OnRecordChanged(record);
    }
  }

 private:
  void Refresh() {
    MdnsMessage message(CreateMessageId(), MessageType::Query);
    message.AddQuestion(question_.value());
    // TODO(yakimakha): Implement known-answer suppression by adding known
    // answers to the question
    sender_->SendMulticast(message);
    refresh_alarm.Schedule(std::bind(&MdnsQuestionTracker::Refresh, this),
                           now_function_() + refresh_delay_);
    refresh_delay_ = refresh_delay_ * kIntervalIncreaseFactor;
    if (refresh_delay_ < kMinimumQueryInterval) {
      refresh_delay_ = kMinimumQueryInterval;
    }
    if (refresh_delay_ > kMaximumQueryInterval) {
      refresh_delay_ = kMaximumQueryInterval;
    }
  }

  const ClockNowFunctionPtr now_function_;
  // TaskRunner* task_runner_;
  MdnsSender* sender_;
  // TODO(yakimakha): Switch to a cancelable task when available
  Alarm refresh_alarm;
  absl::optional<MdnsQuestion> question_;
  Clock::duration refresh_delay_;

  MdnsRandom* mdns_random_;

  std::unordered_set<Delegate*> delegates_;

  std::vector<std::unique_ptr<MdnsRecordTracker>> answers_;
};

MdnsQuerier::MdnsQuerier(MdnsSender* sender,
                         MdnsReceiver* receiver,
                         TaskRunner* task_runner)
    : sender_(sender), receiver_(receiver), task_runner_(task_runner) {
  OSP_DCHECK(sender_ != nullptr);
  OSP_DCHECK(receiver_ != nullptr);
  OSP_DCHECK(task_runner_ != nullptr);

  receiver_->SetResponseDelegate(this);
}

MdnsQuerier::~MdnsQuerier() {
  receiver_->SetResponseDelegate(nullptr);
}

void MdnsQuerier::StartQuery(const DomainName& name,
                             DnsType type,
                             Delegate* delegate) {
  OSP_DCHECK(delegate != nullptr);
  std::pair<DomainName, DnsType> key = std::make_pair(name, type);
  // TODO(yakimakha): Decide when to request unicast response.
  MdnsQuestion question(std::move(name), type, DnsClass::kIN,
                        ResponseType::kMulticast);
  std::lock_guard<std::mutex> lock(queries_mutex_);
  auto find_result = queries_.find(key);
  if (find_result == queries_.end()) {
    auto emplace_result =
        queries_.emplace(key, std::make_unique<MdnsQuestionTracker>(
                                  sender_, task_runner_, Clock::now));
    if (emplace_result.second) {
      emplace_result.first->second->Start(std::move(question));
    }
    // TODO(yakimakha): Do we need to check emplace_result.second here for
    // success? Return if failed to insert?

    find_result = emplace_result.first;
  }
  MdnsQuestionTracker* query = find_result->second.get();
  query->AddDelegate(delegate);
}

void MdnsQuerier::StopQuery(const DomainName& name,
                            DnsType type,
                            Delegate* delegate) {
  OSP_DCHECK(delegate != nullptr);
  std::pair<DomainName, DnsType> key = std::make_pair(name, type);
  std::lock_guard<std::mutex> lock(queries_mutex_);
  // Look up all entries with the matching query key, check if there's an entry
  // for the provided delegate and remove this entry if it is exists.
  auto find_result = queries_.find(key);
  if (find_result != queries_.end()) {
    MdnsQuestionTracker* query = find_result->second.get();
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
    std::pair<DomainName, DnsType> key =
        std::make_pair(record.name(), record.dns_type());
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
