// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp_base/alarm.h"

#include <functional>

#include "platform/api/logging.h"

namespace openscreen {

class Alarm::BackReference {
 public:
  explicit BackReference(Alarm* alarm) : alarm_(alarm) {
    OSP_DCHECK(alarm_);
    OSP_DCHECK(!alarm_->back_reference_);
    alarm_->back_reference_ = this;
  }

  ~BackReference() { Invalidate(); }

  BackReference(BackReference&& other) : alarm_(other.alarm_) {
    other.alarm_ = nullptr;
    if (alarm_) {
      OSP_DCHECK_EQ(alarm_->back_reference_, &other);
      alarm_->back_reference_ = this;
    }
  }

  BackReference& operator=(BackReference&& other) {
    Invalidate();
    alarm_ = other.alarm_;
    other.alarm_ = nullptr;
    if (alarm_) {
      OSP_DCHECK_EQ(alarm_->back_reference_, &other);
      alarm_->back_reference_ = this;
    }
    return *this;
  }

  Alarm* get() const { return alarm_; }

  void Invalidate() {
    if (alarm_) {
      OSP_DCHECK_EQ(alarm_->back_reference_, this);
      alarm_->back_reference_ = nullptr;
      alarm_ = nullptr;
    }
  }

 private:
  Alarm* alarm_;
};

Alarm::Alarm(platform::ClockNowFunctionPtr now_function,
             platform::TaskRunner* task_runner)
    : now_function_(now_function), task_runner_(task_runner) {
  OSP_DCHECK(now_function_);
  OSP_DCHECK(task_runner_);
}

Alarm::~Alarm() {
  if (back_reference_) {
    back_reference_->Invalidate();
  }
}

void Alarm::Disarm() {
  task_ = Task();
}

void Alarm::ArmWithTask(Alarm::Task task,
                        platform::Clock::time_point alarm_time) {
  OSP_DCHECK(task.valid());

  task_ = std::move(task);
  task_invoke_time_ = alarm_time;

  // If there is already a fire-task that will run at or before the desired
  // alarm time, leave it be. If the fire-task would run too late, cancel it.
  if (back_reference_) {
    if (next_fire_time_ <= alarm_time) {
      return;
    }
    back_reference_->Invalidate();
  }

  Schedule(now_function_(), alarm_time);
}

void Alarm::Schedule(platform::Clock::time_point now,
                     platform::Clock::time_point fire_time) {
  OSP_DCHECK(!back_reference_);
  next_fire_time_ = fire_time;
  task_runner_->PostTaskWithDelay(
      std::bind(
          [](const BackReference& alarm) {
            if (Alarm* ptr = alarm.get()) {
              ptr->Fire();
            } else {
              // Do nothing: This firing was canceled in the meantime.
            }
          },
          // Instantiating the BackReference sets |this->back_reference_|.
          BackReference(this)),
      fire_time - now);
}

void Alarm::Fire() {
  OSP_DCHECK(back_reference_);
  back_reference_->Invalidate();

  if (!task_.valid()) {
    return;  // This Alarm was disarmed in the meatime.
  }

  // If this is an early firing, re-schedule for later. This happens if Arm() is
  // called again before an already-armed Alarm (with a later point-in-time).
  const platform::Clock::time_point now = now_function_();
  if (now < task_invoke_time_) {
    Schedule(now, task_invoke_time_);
    return;
  }

  // Move the client Task to the stack before executing, just in case the task
  // itself: a) calls any Alarm methods re-entrantly, or b) causes the
  // destruction of this Alarm instance.
  std::move(task_)();  // WARNING: |this| is no longer valid after this point!
}

}  // namespace openscreen
