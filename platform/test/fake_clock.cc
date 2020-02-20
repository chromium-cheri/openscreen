// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/test/fake_clock.h"

#include <algorithm>

#include "platform/test/fake_task_runner.h"
#include "util/logging.h"

namespace openscreen {

FakeClock::FakeClock(Clock::time_point start_time) {
  OSP_CHECK_EQ(now_, Clock::time_point::min());
  now_ = start_time;
}

FakeClock::~FakeClock() {
  OSP_CHECK(task_runners_.empty());
  now_ = Clock::time_point::min();
}

Clock::time_point FakeClock::now() noexcept {
  OSP_CHECK_NE(now_, Clock::time_point::min()) << "No FakeClock instance!";
  return now_;
}

void FakeClock::Advance(Clock::duration delta) {
  const Clock::time_point stop_time = now() + delta;

  for (;;) {
    // Run tasks at the current time, since this might cause additional delayed
    // tasks to be posted.
    for (FakeTaskRunner* task_runner : task_runners_) {
      task_runner->RunTasksUntilIdle();
    }

    // Find the next "step-to" time, and advance the clock to that point.
    Clock::time_point step_to = Clock::time_point::max();
    for (FakeTaskRunner* task_runner : task_runners_) {
      step_to = std::min(step_to, task_runner->GetResumeTime());
    }
    if (step_to > stop_time) {
      break;  // No tasks are scheduled for the remaining time range.
    }

    OSP_DCHECK_GT(step_to, now_);
    now_ = step_to;
  }

  // Skip over any remaining "dead time."
  now_ = stop_time;
}

void FakeClock::SubscribeToTimeChanges(FakeTaskRunner* task_runner) {
  OSP_CHECK(std::find(task_runners_.begin(), task_runners_.end(),
                      task_runner) == task_runners_.end());
  task_runners_.push_back(task_runner);
}

void FakeClock::UnsubscribeFromTimeChanges(FakeTaskRunner* task_runner) {
  auto it = std::find(task_runners_.begin(), task_runners_.end(), task_runner);
  OSP_CHECK(it != task_runners_.end());
  task_runners_.erase(it);
}

// static
Clock::time_point FakeClock::now_ = Clock::time_point::min();

}  // namespace openscreen
