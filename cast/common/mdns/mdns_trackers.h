// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_MDNS_MDNS_TRACKERS_H_
#define CAST_COMMON_MDNS_MDNS_TRACKERS_H_

#include <random>
#include <unordered_map>

#include "cast/common/mdns/mdns_random.h"
#include "cast/common/mdns/mdns_sender.h"
#include "platform/api/task_runner.h"
#include "platform/api/time.h"
#include "util/alarm.h"

namespace cast {
namespace mdns {

using openscreen::Alarm;
using openscreen::platform::Clock;
using openscreen::platform::ClockNowFunctionPtr;
using openscreen::platform::TaskRunner;

// MdnsRecordTracker manages automatic resending of mDNS queries for
// refreshing records as they reach their expiration time.
class MdnsRecordTracker {
 public:
  // MdnsRecordTracker does not own |sender|, |task_runner| and |random_delay|
  // and expects that the lifetime of these objects exceeds the lifetime of
  // MdnsRecordTracker.
  MdnsRecordTracker(MdnsSender* sender,
                    TaskRunner* task_runner,
                    ClockNowFunctionPtr now_function,
                    MdnsRandom* random_delay);

  MdnsRecordTracker(const MdnsRecordTracker& other) = delete;
  MdnsRecordTracker(MdnsRecordTracker&& other) noexcept = delete;
  ~MdnsRecordTracker() = default;

  MdnsRecordTracker& operator=(const MdnsRecordTracker& other) = delete;
  MdnsRecordTracker& operator=(MdnsRecordTracker&& other) noexcept = delete;

  // Starts sending query messages for the provided record using record's TTL
  // and the time of the call to determine when to send the queries. Returns
  // error with code Error::Code::kOperationInvalid if called on an instance of
  // MdnsRecordTracker that has already been started.
  Error Start(MdnsRecord record);

  // Stops sending query for automatic record refresh. This cancels record
  // expiration notification as well. Returns error
  // with code Error::Code::kOperationInvalid if called on an instance of
  // MdnsRecordTracker that has not yet been started or has already been
  // stopped.
  Error Stop();

  // Returns true if MdnsRecordTracker instance has been started and is
  // automatically refreshing the record, false otherwise.
  bool IsStarted();

 private:
  void SendQuery();
  Clock::time_point GetNextSendTime();

  MdnsSender* const sender_;
  const ClockNowFunctionPtr now_function_;
  Alarm send_alarm_;  // TODO(yakimakha): Use cancelable task when available
  MdnsRandom* const random_delay_;

  // Stores MdnsRecord provided to Start method call.
  absl::optional<MdnsRecord> record_;
  Clock::time_point last_update_time_;
  size_t send_index_ = 0;
};

// MdnsQuestionTracker manages automatic resending of mDNS queries for
// continuous monitoring with exponential back-off as described in RFC 6762
class MdnsQuestionTracker {
 public:
  // MdnsQuestionTracker does not own |sender|, |task_runner| and |random_delay|
  // and expects that the lifetime of these objects exceeds the lifetime of
  // MdnsQuestionTracker.
  MdnsQuestionTracker(MdnsSender* sender,
                      TaskRunner* task_runner,
                      ClockNowFunctionPtr now_function,
                      MdnsRandom* random_delay);

  MdnsQuestionTracker(const MdnsQuestionTracker& other) = delete;
  MdnsQuestionTracker(MdnsQuestionTracker&& other) noexcept = delete;
  ~MdnsQuestionTracker() = default;

  MdnsQuestionTracker& operator=(const MdnsQuestionTracker& other) = delete;
  MdnsQuestionTracker& operator=(MdnsQuestionTracker&& other) noexcept = delete;

  // Starts sending query messages for the provided question. Returns error with
  // code Error::Code::kOperationInvalid if called on an instance of
  // MdnsQuestionTracker that has already been started.
  Error Start(MdnsQuestion question);

  // Stops sending query messages and resets the querying interval. Returns
  // error with code Error::Code::kOperationInvalid if called on an instance of
  // MdnsQuestionTracker that has not yet been started or has already been
  // stopped.
  Error Stop();

  // Returns true if MdnsQuestionTracker instance has been started and is
  // automatically sending queries, false otherwise.
  bool IsStarted();

 private:
  // Sends a query message via MdnsSender and schedules the next resend.
  void SendQuery();

  MdnsSender* const sender_;
  const ClockNowFunctionPtr now_function_;
  Alarm send_alarm_;  // TODO(yakimakha): Use cancelable task when available
  MdnsRandom* const random_delay_;

  // Stores MdnsQuestion provided to Start method call.
  absl::optional<MdnsQuestion> question_;
  Clock::duration send_delay_;
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_TRACKERS_H_
