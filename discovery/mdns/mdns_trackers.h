// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_TRACKERS_H_
#define DISCOVERY_MDNS_MDNS_TRACKERS_H_

#include <unordered_map>

#include "absl/hash/hash.h"
#include "discovery/mdns/mdns_records.h"
#include "platform/api/task_runner.h"
#include "platform/base/error.h"
#include "platform/base/trivial_clock_traits.h"
#include "util/alarm.h"

namespace openscreen {
namespace discovery {

class MdnsRandom;
class MdnsRecord;
class MdnsRecordChangedCallback;
class MdnsSender;

// MdnsTracker is a base class for MdnsRecordTracker and MdnsQuestionTracker for
// the purposes of common code sharing only
class MdnsTracker {
 public:
  // MdnsTracker does not own |sender|, |task_runner| and |random_delay|
  // and expects that the lifetime of these objects exceeds the lifetime of
  // MdnsTracker.
  MdnsTracker(MdnsSender* sender,
              TaskRunner* task_runner,
              ClockNowFunctionPtr now_function,
              MdnsRandom* random_delay);
  MdnsTracker(const MdnsTracker& other) = delete;
  MdnsTracker(MdnsTracker&& other) noexcept = delete;
  MdnsTracker& operator=(const MdnsTracker& other) = delete;
  MdnsTracker& operator=(MdnsTracker&& other) noexcept = delete;
  virtual ~MdnsTracker();

 protected:
  MdnsSender* const sender_;
  TaskRunner* const task_runner_;
  const ClockNowFunctionPtr now_function_;
  Alarm send_alarm_;  // TODO(yakimakha): Use cancelable task when available
  MdnsRandom* const random_delay_;
};

class MdnsQuestionTracker;

// MdnsRecordTracker manages automatic resending of mDNS queries for
// refreshing records as they reach their expiration time.
class MdnsRecordTracker : public MdnsTracker {
 public:
  MdnsRecordTracker(
      MdnsRecord record,
      MdnsSender* sender,
      TaskRunner* task_runner,
      ClockNowFunctionPtr now_function,
      MdnsRandom* random_delay,
      std::function<void(const MdnsRecord&)> record_expired_callback);

  ~MdnsRecordTracker() override;

  // Possible outcomes from updating a tracked record.
  enum class UpdateType {
    kGoodbye,  // The record has a TTL of 0 and will expire.
    kTTLOnly,  // The record updated its TTL only.
    kRdata     // The record updated its RDATA.
  };

  // Updates record tracker with the new record:
  // 1. Resets TTL to the value specified in |new_record|.
  // 2. Schedules expiration in case of a goodbye record.
  // Returns Error::Code::kParameterInvalid if new_record is not a valid update
  // for the current tracked record.
  ErrorOr<UpdateType> Update(const MdnsRecord& new_record);

  // Adds or removed a question which this record answers.
  bool AddAssociatedQuery(MdnsQuestionTracker* question_tracker);
  bool RemoveAssociatedQuery(MdnsQuestionTracker* question_tracker);

  // Sets record to expire after 1 seconds as per RFC 6762
  void ExpireSoon();

  // Returns true if half of the record's TTL has passed, and false otherwise.
  // Half is used due to specifications in RFC 6762 section 7.1.
  bool IsNearingExpiry();

  // Returns a reference to the tracked record.
  const MdnsRecord& record() const { return record_; }

 private:
  // This class must be able to call private methods AddAssociatedRecord(),
  // RemoveAssociatedRecord() and SendQuery();
  friend class MdnsQuestionTracker;

  bool AddAssociatedQuery(MdnsQuestionTracker* question_tracker,
                          bool updated_query);
  bool RemoveAssociatedQuery(MdnsQuestionTracker* question_tracker,
                             bool updated_query);

  void SendQuery();
  Clock::time_point GetNextSendTime();

  // Stores MdnsRecord provided to Start method call.
  MdnsRecord record_;
  // A point in time when the record was received and the tracking has started.
  Clock::time_point start_time_;
  // Number of times record refresh has been attempted.
  size_t attempt_count_ = 0;
  std::function<void(const MdnsRecord&)> record_expired_callback_;

  // The set of records answering questions associated with this record.
  std::vector<MdnsQuestionTracker*> associated_questions_;
};

// MdnsQuestionTracker manages automatic resending of mDNS queries for
// continuous monitoring with exponential back-off as described in RFC 6762.
class MdnsQuestionTracker : public MdnsTracker {
 public:
  MdnsQuestionTracker(MdnsQuestion question,
                      MdnsSender* sender,
                      TaskRunner* task_runner,
                      ClockNowFunctionPtr now_function,
                      MdnsRandom* random_delay);

  ~MdnsQuestionTracker() override;

  // Adds or removed an answer to a the question posed by this tracker.
  bool AddAssociatedRecord(MdnsRecordTracker* record_tracker);
  bool RemoveAssociatedRecord(MdnsRecordTracker* record_tracker);

  // Returns a reference to the tracked question.
  const MdnsQuestion& question() const { return question_; }

  void stop_periodic_queries() { is_querying_periodically = false; }

 private:
  using RecordKey = std::tuple<DomainName, DnsType, DnsClass>;

  // This class must be able to call private methods AddAssociatedQuery() and
  // RemoveAssociatedQuery() private methods.
  friend class MdnsRecordTracker;

  bool AddAssociatedRecord(MdnsRecordTracker* record_tracker,
                           bool update_record);
  bool RemoveAssociatedRecord(MdnsRecordTracker* record_tracker,
                              bool update_record);

  // Determines if all answers to this query have been received.
  bool HasReceivedAllResponses();

  // Sends a query message via MdnsSender and schedules the next resend.
  void SendQuery(bool is_on_demand_query);

  // Stores MdnsQuestion provided to Start method call.
  MdnsQuestion question_;

  // A delay between the currently scheduled and the next queries.
  Clock::duration send_delay_;

  // The set of records answering questions associated with this record.
  std::vector<MdnsRecordTracker*> associated_records_;

  // Last time that this tracker's question was asked.
  TrivialClockTraits::time_point last_send_time_;

  bool is_querying_periodically = true;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_MDNS_TRACKERS_H_
