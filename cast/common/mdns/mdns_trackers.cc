// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_trackers.h"

#include <array>

namespace cast {
namespace mdns {

namespace {
// RFC 6762 Section 5.2
// https://tools.ietf.org/html/rfc6762#section-5.2
constexpr std::array<double, 5> kTtlFractions = {0.80, 0.85, 0.90, 0.95, 1.00};
}  // namespace

MdnsRecordTracker::MdnsRecordTracker(MdnsSender* sender,
                                     TaskRunner* task_runner,
                                     ClockNowFunctionPtr now_function,
                                     MdnsRandom* random_delay)
    : sender_(sender),
      now_function_(now_function),
      send_alarm_(now_function, task_runner),
      random_delay_(random_delay) {
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
  send_index_ = 0;
  send_alarm_.Schedule(std::bind(&MdnsRecordTracker::SendQuery, this),
                       GetNextSendTime());
  return Error::None();
}

Error MdnsRecordTracker::Stop() {
  if (!record_.has_value()) {
    return Error::Code::kOperationInvalid;
  }

  send_alarm_.Cancel();
  record_.reset();
  return Error::None();
}

bool MdnsRecordTracker::IsStarted() {
  return record_.has_value();
};

void MdnsRecordTracker::SendQuery() {
  if (send_index_ < kTtlFractions.size()) {
    const MdnsRecord& record = record_.value();
    MdnsQuestion question(record.name(), record.dns_type(),
                          record.record_class(), ResponseType::kMulticast);
    MdnsMessage message(CreateMessageId(), MessageType::Query);
    message.AddQuestion(question);
    sender_->SendMulticast(message);
    send_alarm_.Schedule(std::bind(&MdnsRecordTracker::SendQuery, this),
                         GetNextSendTime());
  } else {
    // TODO(yakimakha): Notify owner that the record has expired
  }
}

Clock::time_point MdnsRecordTracker::GetNextSendTime() {
  OSP_DCHECK(send_index_ < kTtlFractions.size());

  double ttl_fraction =
      kTtlFractions[send_index_++] + random_delay_->GetRecordTtlVariation();

  OSP_DCHECK(ttl_fraction <= 1.0);

  Clock::duration delay = std::chrono::duration_cast<Clock::duration>(
      record_.value().ttl() * ttl_fraction);
  return last_update_time_ + delay;
}

namespace {
// RFC 6762 Section 5.2
// https://tools.ietf.org/html/rfc6762#section-5.2
constexpr int kIntervalIncreaseFactor = 2;
constexpr std::chrono::seconds kMinimumQueryInterval{1};
constexpr std::chrono::minutes kMaximumQueryInterval{60};
}  // namespace

MdnsQuestionTracker::MdnsQuestionTracker(MdnsSender* sender,
                                         TaskRunner* task_runner,
                                         ClockNowFunctionPtr now_function,
                                         MdnsRandom* random_delay)
    : sender_(sender),
      now_function_(now_function),
      send_alarm_(now_function, task_runner),
      random_delay_(random_delay) {
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
  send_delay_ = kMinimumQueryInterval;
  // The initial query has to be sent after a random delay of 20-120
  // milliseconds.
  Clock::duration delay = random_delay_->GetInitialQueryDelay();
  send_alarm_.Schedule(std::bind(&MdnsQuestionTracker::SendQuery, this),
                       now_function_() + delay);
  return Error::None();
}

Error MdnsQuestionTracker::Stop() {
  if (!question_.has_value()) {
    return Error::Code::kOperationInvalid;
  }

  send_alarm_.Cancel();
  question_.reset();
  return Error::None();
}

bool MdnsQuestionTracker::IsStarted() {
  return question_.has_value();
};

void MdnsQuestionTracker::SendQuery() {
  MdnsMessage message(CreateMessageId(), MessageType::Query);
  message.AddQuestion(question_.value());
  // TODO(yakimakha): Implement known-answer suppression by adding known
  // answers to the question
  sender_->SendMulticast(message);
  send_alarm_.Schedule(std::bind(&MdnsQuestionTracker::SendQuery, this),
                       now_function_() + send_delay_);
  send_delay_ = send_delay_ * kIntervalIncreaseFactor;
  if (send_delay_ < kMinimumQueryInterval) {
    send_delay_ = kMinimumQueryInterval;
  }
  if (send_delay_ > kMaximumQueryInterval) {
    send_delay_ = kMaximumQueryInterval;
  }
}

}  // namespace mdns
}  // namespace cast