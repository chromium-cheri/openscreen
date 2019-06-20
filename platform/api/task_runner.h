// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TASK_RUNNER_H_
#define PLATFORM_API_TASK_RUNNER_H_

#include <future>

#include "absl/types/optional.h"
#include "platform/api/time.h"

namespace openscreen {
namespace platform {

// A thread-safe API surface that allows for posting tasks. The underlying
// implementation may be single or multi-threaded, and all complication should
// be handled by the implementation class. It is the expectation of this API
// that the underlying impl gives the following guarantees:
// (1) Tasks shall not overlap in time/CPU.
// (2) Tasks shall run sequentially, e.g. posting task A then B implies
//     that A shall run before B.
// NOTE: We do not make any assumptions about what thread tasks shall run on.
class TaskRunner {
 public:
  using Task = std::packaged_task<void() noexcept>;

  virtual ~TaskRunner() = default;

  // Takes any callable target (function, lambda-expression, std::bind result,
  // etc.) that should be run at the first convenient time.
  template <typename Functor>
  inline void PostTask(Functor f) {
    PostPackagedTask(Task(std::move(f)));
  }

  // Takes any callable target (function, lambda-expression, std::bind result,
  // etc.) that should be run no sooner than |delay| time from now. Note that
  // the Task might run after an additional delay, especially under heavier
  // system load. There is no deadline concept.
  template <typename Functor>
  inline void PostTaskWithDelay(Functor f, Clock::duration delay) {
    PostPackagedTaskWithDelay(Task(std::move(f)), delay);
  }

  // Posts a delayed task that will run repeatedly. The result of the function
  // object will determine if the task should be reposted, in that it will be
  // reposted if and only if the result is nonzero.
  // NOTE: Because the same object is used for repeated execution, a function
  // must be used rather than an arbitrary Functor as above.
  inline void PostRepeatedTask(
      std::function<absl::optional<Clock::duration>()> function,
      Clock::duration delay = Clock::duration(0)) {
    PostTaskWithDelay(RepeatingFunction(this, function), delay);
  }

  // Implementations should provide the behavior explained in the comments above
  // for PostTask[WithDelay](). Client code may also call these directly when
  // passing an existing Task object.
  virtual void PostPackagedTask(Task task) = 0;
  virtual void PostPackagedTaskWithDelay(Task task, Clock::duration delay) = 0;

  // Tasks will only be executed if RunUntilStopped has been called, and
  // RequestStopSoon has not. If the is_async parameter is set to true, a new
  // thread will be created to execute tasks on. Else, calling
  // "RunUntilStopped()" will block whatever thread you are calling it on.
  virtual void RunUntilStopped(bool is_async = true) = 0;

  // Thread-safe method for requesting the TaskRunnerImpl to stop running. This
  // sets a flag that will get checked in the run loop, typically after
  // completing the current task.
  virtual void RequestStopSoon() = 0;

 private:
  // Class used to post the same task more than once.
  class RepeatingFunction {
   public:
    // Creates a new task that will be posted repeatedly to the task runner. If
    // the function returns a valid Clock::duration, it will be reposted with
    // that delay, and will not be reposted if absl::nullopt is instead
    // returned.
    RepeatingFunction(TaskRunner* task_runner,
                      std::function<absl::optional<Clock::duration>()> function)
        : task_runner_(task_runner), function_(function) {}

    void operator()() {
      absl::optional<Clock::duration> delay = function_();
      if (delay.has_value()) {
        task_runner_->PostRepeatedTask(function_, delay.value());
      }
    }

   private:
    TaskRunner* task_runner_;
    std::function<absl::optional<Clock::duration>()> function_;
  };
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_TASK_RUNNER_H_
