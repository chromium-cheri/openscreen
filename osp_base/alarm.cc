// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp_base/alarm.h"

#include "platform/api/logging.h"

namespace openscreen {

class Alarm::CancelableFunctor {
 public:
  explicit CancelableFunctor(Alarm* alarm) : alarm_(alarm) {
    OSP_DCHECK(alarm_);
    OSP_DCHECK(!alarm_->pending_fire_);
    alarm_->pending_fire_ = this;
  }

  ~CancelableFunctor() { Cancel(); }

  CancelableFunctor(CancelableFunctor&& other) : alarm_(other.alarm_) {
    other.alarm_ = nullptr;
    if (alarm_) {
      OSP_DCHECK_EQ(alarm_->pending_fire_, &other);
      alarm_->pending_fire_ = this;
    }
  }

  CancelableFunctor& operator=(CancelableFunctor&& other) {
    Cancel();
    alarm_ = other.alarm_;
    other.alarm_ = nullptr;
    if (alarm_) {
      OSP_DCHECK_EQ(alarm_->pending_fire_, &other);
      alarm_->pending_fire_ = this;
    }
    return *this;
  }

  void operator()() noexcept {
    if (alarm_) {
      OSP_DCHECK_EQ(alarm_->pending_fire_, this);
      alarm_->pending_fire_ = nullptr;
      alarm_->Fire();
      alarm_ = nullptr;
    }
  }

  void Cancel() {
    if (alarm_) {
      OSP_DCHECK_EQ(alarm_->pending_fire_, this);
      alarm_->pending_fire_ = nullptr;
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
  if (pending_fire_) {
    pending_fire_->Cancel();
  }
}

void Alarm::Disarm() {
  client_task_ = Task();
}

void Alarm::ArmWithTask(Alarm::Task task,
                        platform::Clock::time_point alarm_time) {
  OSP_DCHECK(task.valid());

  client_task_ = std::move(task);
  client_task_invoke_time_ = alarm_time;

  // If there is already a fire-task that will run at or before the desired
  // alarm time, leave it be. If the fire-task would run too late, cancel it.
  if (pending_fire_) {
    if (next_fire_time_ <= alarm_time) {
      return;
    }
    pending_fire_->Cancel();
  }

  Schedule(now_function_(), alarm_time);
}

void Alarm::Schedule(platform::Clock::time_point now,
                     platform::Clock::time_point fire_time) {
  OSP_DCHECK(!pending_fire_);
  next_fire_time_ = fire_time;
  // Note: Instantiating the CancelableFunctor below sets |this->pending_fire_|.
  task_runner_->PostTaskWithDelay(CancelableFunctor(this), fire_time - now);
}

void Alarm::Fire() {
  if (!client_task_.valid()) {
    return;  // This Alarm was disarmed in the meatime.
  }

  // If this is an early firing, re-schedule for later. This happens if Arm() is
  // called again before an already-armed Alarm (with a later point-in-time).
  const platform::Clock::time_point now = now_function_();
  if (now < client_task_invoke_time_) {
    Schedule(now, client_task_invoke_time_);
    return;
  }

  // Move the client Task to the stack before executing, just in case the task
  // itself: a) calls any Alarm methods re-entrantly, or b) causes the
  // destruction of this Alarm instance.
  std::move(client_task_)();  // WARNING: |this| is not valid after this point!
}

}  // namespace openscreen
