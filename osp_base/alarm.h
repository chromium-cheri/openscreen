// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_BASE_ALARM_H_
#define OSP_BASE_ALARM_H_

#include <future>

#include "platform/api/task_runner.h"
#include "platform/api/time.h"

namespace openscreen {

// A simple mechanism for running one Task in the future, but also allow for
// canceling the Task before it runs and/or re-scheduling a replacement Task to
// run at a different time. This mechanism is also scoped to its lifetime: If an
// Alarm is destroyed while it is armed (and before it fires), the Task is
// automatically canceled.
//
// Example use case: When using a TaskRunner, an object can safely schedule a
// call back into one of its instance methods (without the possibility of the
// Task executing after the object is destroyed).
//
// Design: In order to support efficient, arbitrary disarming and re-arming by
// the client, the Alarm posts an internal "fire" Task to the TaskRunner which,
// when invoked, then checks to see: a) if the Alarm instance still exists; b)
// whether the invocation time of the client's Task has changed; and c) whether
// the Alarm was disarmed in the meantime. From this, it either: a) does
// nothing; b) re-posts a new fire-task to the TaskRunner, to run the client's
// Task later; or c) runs the client's Task. It is safe for the client's Task to
// make re-entrant calls into all Alarm methods.
class Alarm {
 public:
  using Task = platform::TaskRunner::Task;

  Alarm(platform::ClockNowFunctionPtr now_function,
        platform::TaskRunner* task_runner);
  ~Alarm();

  Alarm(const Alarm&) = delete;
  Alarm& operator=(const Alarm&) = delete;

  // Schedule the |functor| to be invoked at |alarm_time|. Cancels an existing
  // arming, if this Alarm was already armed but had not fired yet. The Functor
  // can be any callable target (e.g., function, lambda-expression, std::bind
  // result, etc.).
  template <typename Functor>
  inline void Arm(Functor functor, platform::Clock::time_point alarm_time) {
    ArmWithTask(Task(std::move(functor)), alarm_time);
  }

  // Cancels an unfired task from running, or no-op.
  void Disarm();

  // See comments for Arm(). Generally, callers will want to call Arm() instead
  // of this, for more-convenient caller-side syntax, unless they already have a
  // Task to pass-in.
  void ArmWithTask(Task task, platform::Clock::time_point alarm_time);

 private:
  // A move-only wrapper that holds a raw pointer back to |this| and tracks
  // whether the Alarm it points to is destroyed. The fire-task uses this to
  // safely no-op out when appropriate.
  class BackReference;

  // Posts a delayed call to Fire() to the TaskRunner.
  void Schedule(platform::Clock::time_point now,
                platform::Clock::time_point fire_time);

  // Examines whether to invoke the client's Task now, or re-schedule a new
  // fire-task to do it later, or just do nothing. See class-level comments.
  void Fire();

  const platform::ClockNowFunctionPtr now_function_;
  platform::TaskRunner* const task_runner_;

  // This is the task the client wants to have run at a specific point-in-time.
  // This is NOT the task that Alarm provides to the TaskRunner.
  Task task_;
  platform::Clock::time_point task_invoke_time_{};

  // When non-null, there is a fire-task in the TaskRunner's queue.
  BackReference* back_reference_ = nullptr;

  // When the fire-task is supposed to execute. It may possibly execute a late,
  // if the TaskRunner is falling behind.
  platform::Clock::time_point next_fire_time_{};
};

}  // namespace openscreen

#endif  // OSP_BASE_ALARM_H_
