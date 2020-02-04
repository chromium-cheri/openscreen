// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_trackers.h"

#include <array>

#include "discovery/mdns/mdns_random.h"
#include "discovery/mdns/mdns_record_changed_callback.h"
#include "discovery/mdns/mdns_sender.h"
#include "util/std_util.h"

namespace openscreen {
namespace discovery {

namespace {

// RFC 6762 Section 5.2
// https://tools.ietf.org/html/rfc6762#section-5.2

// Attempt to refresh a record should be performed at 80%, 85%, 90% and 95% TTL.
constexpr double kTtlFractions[] = {0.80, 0.85, 0.90, 0.95, 1.00};

// Intervals between successive queries must increase by at least a factor of 2.
constexpr int kIntervalIncreaseFactor = 2;

// The interval between the first two queries must be at least one second.
constexpr std::chrono::seconds kMinimumQueryInterval{1};

// The querier may cap the question refresh interval to a maximum of 60 minutes.
constexpr std::chrono::minutes kMaximumQueryInterval{60};

// RFC 6762 Section 10.1
// https://tools.ietf.org/html/rfc6762#section-10.1

// A goodbye record is a record with TTL of 0.
bool IsGoodbyeRecord(const MdnsRecord& record) {
  return record.ttl() == std::chrono::seconds{0};
}

// RFC 6762 Section 10.1
// https://tools.ietf.org/html/rfc6762#section-10.1
// In case of a goodbye record, the querier should set TTL to 1 second
constexpr std::chrono::seconds kGoodbyeRecordTtl{1};

}  // namespace

MdnsTracker::MdnsTracker(MdnsSender* sender,
                         TaskRunner* task_runner,
                         ClockNowFunctionPtr now_function,
                         MdnsRandom* random_delay)
    : sender_(sender),
      task_runner_(task_runner),
      now_function_(now_function),
      send_alarm_(now_function, task_runner),
      random_delay_(random_delay) {
  OSP_DCHECK(task_runner_);
  OSP_DCHECK(now_function_);
  OSP_DCHECK(random_delay_);
  OSP_DCHECK(sender_);
}

MdnsTracker::~MdnsTracker() {
  send_alarm_.Cancel();
}

MdnsRecordTracker::MdnsRecordTracker(
    MdnsRecord record,
    MdnsSender* sender,
    TaskRunner* task_runner,
    ClockNowFunctionPtr now_function,
    MdnsRandom* random_delay,
    std::function<void(const MdnsRecord&)> record_expired_callback)
    : MdnsTracker(sender, task_runner, now_function, random_delay),
      record_(std::move(record)),
      start_time_(now_function_()),
      record_expired_callback_(record_expired_callback) {
  OSP_DCHECK(record_expired_callback);

  send_alarm_.Schedule([this] { SendQuery(); }, GetNextSendTime());
}

MdnsRecordTracker::~MdnsRecordTracker() {
  for (auto* query : associated_questions_) {
    query->RemoveAssociatedRecord(this, false);
  }
}

ErrorOr<MdnsRecordTracker::UpdateType> MdnsRecordTracker::Update(
    const MdnsRecord& new_record) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  bool has_same_rdata = record_.rdata() == new_record.rdata();

  // Goodbye records must have the same RDATA but TTL of 0.
  // RFC 6762 Section 10.1
  // https://tools.ietf.org/html/rfc6762#section-10.1
  if ((record_.dns_type() != new_record.dns_type()) ||
      (record_.dns_class() != new_record.dns_class()) ||
      (record_.name() != new_record.name()) ||
      (IsGoodbyeRecord(new_record) && !has_same_rdata)) {
    // The new record has been passed to a wrong tracker.
    return Error::Code::kParameterInvalid;
  }

  UpdateType result = UpdateType::kGoodbye;
  if (IsGoodbyeRecord(new_record)) {
    record_ = MdnsRecord(new_record.name(), new_record.dns_type(),
                         new_record.dns_class(), new_record.record_type(),
                         kGoodbyeRecordTtl, new_record.rdata());

    // Goodbye records do not need to be re-queried, set the attempt count to
    // the last item, which is 100% of TTL, i.e. record expiration.
    attempt_count_ = countof(kTtlFractions) - 1;
  } else {
    record_ = new_record;
    attempt_count_ = 0;
    result = has_same_rdata ? UpdateType::kTTLOnly : UpdateType::kRdata;
  }

  start_time_ = now_function_();
  send_alarm_.Schedule([this] { SendQuery(); }, GetNextSendTime());

  return result;
}

bool MdnsRecordTracker::AddAssociatedQuery(
    MdnsQuestionTracker* question_tracker) {
  return AddAssociatedQuery(question_tracker, true);
}

bool MdnsRecordTracker::RemoveAssociatedQuery(
    MdnsQuestionTracker* question_tracker) {
  return RemoveAssociatedQuery(question_tracker, true);
}

bool MdnsRecordTracker::AddAssociatedQuery(
    MdnsQuestionTracker* question_tracker,
    bool update_query) {
  OSP_DCHECK(question_tracker);
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  auto it = std::find(associated_questions_.begin(),
                      associated_questions_.end(), question_tracker);
  if (it != associated_questions_.end()) {
    return false;
  }

  if (update_query) {
    question_tracker->AddAssociatedRecord(this, false);
  }
  associated_questions_.push_back(question_tracker);
  return true;
}

bool MdnsRecordTracker::RemoveAssociatedQuery(
    MdnsQuestionTracker* question_tracker,
    bool update_query) {
  OSP_DCHECK(question_tracker);
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  auto it = std::find(associated_questions_.begin(),
                      associated_questions_.end(), question_tracker);
  if (it == associated_questions_.end()) {
    return false;
  }

  if (update_query) {
    question_tracker->RemoveAssociatedRecord(this, false);
  }
  associated_questions_.erase(it);
  return true;
}

void MdnsRecordTracker::ExpireSoon() {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  record_ =
      MdnsRecord(record_.name(), record_.dns_type(), record_.dns_class(),
                 record_.record_type(), kGoodbyeRecordTtl, record_.rdata());

  // Set the attempt count to the last item, which is 100% of TTL, i.e. record
  // expiration, to prevent any re-queries
  attempt_count_ = countof(kTtlFractions) - 1;
  start_time_ = now_function_();
  send_alarm_.Schedule([this] { SendQuery(); }, GetNextSendTime());
}

bool MdnsRecordTracker::IsNearingExpiry() {
  return (now_function_() - start_time_) > record_.ttl() / 2;
}

void MdnsRecordTracker::SendQuery() {
  const Clock::time_point expiration_time = start_time_ + record_.ttl();
  const bool is_expired = (now_function_() >= expiration_time);
  if (!is_expired) {
    for (MdnsQuestionTracker* query : associated_questions_) {
      query->SendQuery(true);
    }
    send_alarm_.Schedule([this] { MdnsRecordTracker::SendQuery(); },
                         GetNextSendTime());
  } else {
    record_expired_callback_(record_);
  }
}

Clock::time_point MdnsRecordTracker::GetNextSendTime() {
  OSP_DCHECK(attempt_count_ < countof(kTtlFractions));

  double ttl_fraction = kTtlFractions[attempt_count_++];

  // Do not add random variation to the expiration time (last fraction of TTL)
  if (attempt_count_ != countof(kTtlFractions)) {
    ttl_fraction += random_delay_->GetRecordTtlVariation();
  }

  const Clock::duration delay =
      std::chrono::duration_cast<Clock::duration>(record_.ttl() * ttl_fraction);
  return start_time_ + delay;
}

MdnsQuestionTracker::MdnsQuestionTracker(MdnsQuestion question,
                                         MdnsSender* sender,
                                         TaskRunner* task_runner,
                                         ClockNowFunctionPtr now_function,
                                         MdnsRandom* random_delay,
                                         bool is_one_shot_query)
    : MdnsTracker(sender, task_runner, now_function, random_delay),
      question_(std::move(question)),
      send_delay_(kMinimumQueryInterval),
      is_one_shot_query_(is_one_shot_query) {
  // Initialize the last send time to time_point::min() so that the next call to
  // SendQuery() is guaranteed to query the network.
  last_send_time_ = TrivialClockTraits::time_point::min();

  // The initial query has to be sent after a random delay of 20-120
  // milliseconds.
  if (is_one_shot_query) {
    task_runner_->PostTask([this] { MdnsQuestionTracker::SendQuery(false); });
  } else {
    send_alarm_.ScheduleFromNow(
        [this] { MdnsQuestionTracker::SendQuery(false); },
        random_delay_->GetInitialQueryDelay());
  }
}

MdnsQuestionTracker::~MdnsQuestionTracker() {
  for (auto* record : associated_records_) {
    record->RemoveAssociatedQuery(this, false);
  }
}

bool MdnsQuestionTracker::AddAssociatedRecord(
    MdnsRecordTracker* record_tracker) {
  return AddAssociatedRecord(record_tracker, true);
}

bool MdnsQuestionTracker::RemoveAssociatedRecord(
    MdnsRecordTracker* record_tracker) {
  return RemoveAssociatedRecord(record_tracker, true);
}

bool MdnsQuestionTracker::AddAssociatedRecord(MdnsRecordTracker* record_tracker,
                                              bool updated_query) {
  OSP_DCHECK(record_tracker);
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  auto it = std::find(associated_records_.begin(), associated_records_.end(),
                      record_tracker);
  if (it != associated_records_.end()) {
    return false;
  }

  if (updated_query) {
    record_tracker->AddAssociatedQuery(this, false);
  }
  associated_records_.push_back(record_tracker);
  return true;
}

bool MdnsQuestionTracker::RemoveAssociatedRecord(
    MdnsRecordTracker* record_tracker,
    bool updated_query) {
  OSP_DCHECK(record_tracker);
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  auto it = std::find(associated_records_.begin(), associated_records_.end(),
                      record_tracker);
  if (it == associated_records_.end()) {
    return false;
  }

  if (updated_query) {
    record_tracker->RemoveAssociatedQuery(this, false);
  }
  associated_records_.erase(it);
  return true;
}

void MdnsQuestionTracker::SendQuery(bool is_on_demand_query) {
  // NOTE: The RFC does not specify the minimum interval between queries for
  // multiple records of the same query when initiated for different reasons
  // (such as for different record refreshes or for one record refresh and the
  // periodic re-querying for a continuous query). For this reason, a constant
  // outside of scope of the RFC has been chosen.
  TrivialClockTraits::time_point now = now_function_();
  if (now < last_send_time_ + kMinimumQueryInterval) {
    return;
  }
  last_send_time_ = now;

  MdnsMessage message(CreateMessageId(), MessageType::Query);
  message.AddQuestion(question_);

  // Send the message and additional known answer packets as needed.
  for (auto it = associated_records_.begin();
       it != associated_records_.end();) {
    if ((*it)->IsNearingExpiry()) {
      it++;
      continue;
    }

    if (message.CanAddRecord((*it)->record())) {
      message.AddAnswer((*it++)->record());
    } else if (message.questions().empty() && message.answers().empty()) {
      // This case should never happen, because it means a record is too large
      // to fit into its own message.
      OSP_LOG << "Encountered unreasonably large message in cache. Skipping "
              << "known answer in suppressions...";
      it++;
    } else {
      message.set_truncated();
      sender_->SendMulticast(message);
      message = MdnsMessage(CreateMessageId(), MessageType::Query);
    }
  }
  sender_->SendMulticast(message);

  // Reschedule this task to run again per FRC spec.
  if (!is_on_demand_query && !is_one_shot_query_) {
    send_alarm_.ScheduleFromNow(
        [this] { MdnsQuestionTracker::SendQuery(false); }, send_delay_);
    send_delay_ = send_delay_ * kIntervalIncreaseFactor;
    if (send_delay_ > kMaximumQueryInterval) {
      send_delay_ = kMaximumQueryInterval;
    }
  }
}

}  // namespace discovery
}  // namespace openscreen
