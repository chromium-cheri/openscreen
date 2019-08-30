// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_trackers.h"

namespace cast {
namespace mdns {

namespace {
// RFC 6762 Section 5.2
// https://tools.ietf.org/html/rfc6762#section-5.2
constexpr double kFirstRefreshTtlPercent = 80.0;
constexpr double kSecondRefreshTtlPercent = 85.0;
constexpr double kThirdRefreshTtlPercent = 90.0;
constexpr double kFourthRefreshTtlPercent = 95.0;
}  // namespace

MdnsRecordTracker::MdnsRecordTracker(TaskRunner* task_runner,
                                     ClockNowFunctionPtr now_function,
                                     MdnsRandom* random_delay,
                                     MdnsSender* sender)
    : now_function_(now_function),
      expired_alarm_(now_function, task_runner),
      refresh_alarm_(now_function, task_runner),
      random_delay_(random_delay),
      sender_(sender) {
  OSP_DCHECK(task_runner);
  OSP_DCHECK(now_function);
  OSP_DCHECK(random_delay);
  OSP_DCHECK(sender);
}

Error MdnsRecordTracker::Start(MdnsRecord record) {
  if (record_.has_value()) {
    return Error::Code::kOperationInvalid;
  }

  record_ = std::move(record);
  last_update_time_ = now_function_();
  expired_alarm_.Schedule(
      [this] { Expire(); },
      last_update_time_ + std::chrono::seconds(record.ttl()));
  refresh_alarm_.Schedule(
      [this] { Refresh(); },
      last_update_time_ + std::chrono::seconds(record.ttl()));  // 0.8 ?
}

Error MdnsRecordTracker::Stop() {
  if (!record_.has_value()) {
    return Error::Code::kOperationInvalid;
  }

  expired_alarm_.Cancel();
  refresh_alarm_.Cancel();
  record_.reset();
}

bool MdnsRecordTracker::IsTracking() {
  return record_.has_value();
};

// TODO: re-query only with the correct type, not ANY
void MdnsRecordTracker::Expire() {
  // Call record changed with record deleted
  // Schedule delayed deletion if necessary (in Querier)
}

void MdnsRecordTracker::Refresh() {
  expired_alarm_.Cancel();  // Do we need to cancel this?
  refresh_alarm_.Cancel();
  // Call send refresh
  // Schedule another refresh
}

namespace {
// RFC 6762 Section 5.2
// https://tools.ietf.org/html/rfc6762#section-5.2
constexpr int kIntervalIncreaseFactor = 2;
constexpr std::chrono::seconds kMinimumQueryInterval{1};
constexpr std::chrono::minutes kMaximumQueryInterval{60};
}  // namespace

MdnsQuestionTracker::MdnsQuestionTracker(TaskRunner* task_runner,
                                         ClockNowFunctionPtr now_function,
                                         MdnsRandom* random_delay,
                                         MdnsSender* sender)
    : now_function_(now_function),
      refresh_alarm_(now_function, task_runner),
      random_delay_(random_delay),
      sender_(sender) {
  OSP_DCHECK(task_runner);
  OSP_DCHECK(now_function);
  OSP_DCHECK(random_delay);
  OSP_DCHECK(sender);
}

Error MdnsQuestionTracker::Start(MdnsQuestion question) {
  if (question_.has_value()) {
    return Error::Code::kOperationInvalid;
  }

  question_ = std::move(question);
  refresh_delay_ = kMinimumQueryInterval;
  // The initial query has to be sent after a random delay of 20-120
  // milliseconds.
  Clock::duration delay = random_delay_->GetInitialQueryDelay();
  refresh_alarm_.Schedule(std::bind(&MdnsQuestionTracker::Refresh, this),
                          now_function_() + delay);
  return Error::None();
}

Error MdnsQuestionTracker::Stop() {
  if (!question_.has_value()) {
    return Error::Code::kOperationInvalid;
  }

  refresh_alarm_.Cancel();
  question_.reset();
  return Error::None();
}

bool MdnsQuestionTracker::IsTracking() {
  return question_.has_value();
};

void MdnsQuestionTracker::Refresh() {
  MdnsMessage message(CreateMessageId(), MessageType::Query);
  message.AddQuestion(question_.value());
  // TODO(yakimakha): Implement known-answer suppression by adding known
  // answers to the question
  sender_->SendMulticast(message);
  refresh_alarm_.Schedule(std::bind(&MdnsQuestionTracker::Refresh, this),
                          now_function_() + refresh_delay_);
  refresh_delay_ = refresh_delay_ * kIntervalIncreaseFactor;
  if (refresh_delay_ < kMinimumQueryInterval) {
    refresh_delay_ = kMinimumQueryInterval;
  }
  if (refresh_delay_ > kMaximumQueryInterval) {
    refresh_delay_ = kMaximumQueryInterval;
  }
}

}  // namespace mdns
}  // namespace cast