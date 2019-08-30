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

using TaskRunner = openscreen::platform::TaskRunner;
using Clock = openscreen::platform::Clock;
using ClockNowFunctionPtr = openscreen::platform::ClockNowFunctionPtr;
using Alarm = openscreen::Alarm;

class MdnsQuestionTracker {
 public:
  MdnsQuestionTracker(TaskRunner* task_runner,
                      ClockNowFunctionPtr now_function,
                      MdnsRandom* random_delay,
                      MdnsSender* sender);

  MdnsQuestionTracker(const MdnsQuestionTracker& other) = delete;
  MdnsQuestionTracker(MdnsQuestionTracker&& other) noexcept = delete;
  ~MdnsQuestionTracker() = default;

  MdnsQuestionTracker& operator=(const MdnsQuestionTracker& other) = delete;
  MdnsQuestionTracker& operator=(MdnsQuestionTracker&& other) noexcept = delete;

  Error Start(MdnsQuestion question);
  Error Stop();
  bool IsTracking();

 private:
  void Refresh();

  const ClockNowFunctionPtr now_function_;
  Alarm refresh_alarm_;  // TODO(yakimakha): Use cancelable task when available
  MdnsRandom* random_delay_;
  MdnsSender* sender_;

  absl::optional<MdnsQuestion> question_;
  Clock::duration refresh_delay_;
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_TRACKERS_H_
