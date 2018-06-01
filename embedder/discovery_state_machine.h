// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_DISCOVERY_STATE_MACHINE_H_
#define EMBEDDER_DISCOVERY_STATE_MACHINE_H_

#include <ostream>
#include <vector>

namespace openscreen {

class DiscoveryStateMachine {
 public:
  enum class State {
    kStopped = 0,
    kStarting,
    kRunning,
    kStopping,
    kSearchingFromRunning,
    kSearchingFromSuspended,
    kSuspended,
  };

  enum class StateTransition {
    kNone = -1,
    kStart = 0,
    kStop,
    kSuspend,
    kResume,
    kSearchNow,
    kStartSuspended,
  };

  DiscoveryStateMachine();
  ~DiscoveryStateMachine();

  std::vector<StateTransition> TakeNewStateTransitions();
  StateTransition SetState(State state);

  bool Start();
  bool StartAndSuspend();
  bool Stop();
  bool Suspend();
  bool Resume();
  bool SearchNow();

  State state() const { return state_; }

 private:
  State state_ = State::kStopped;
  State state_after_transitions_ = State::kStopped;
  std::vector<StateTransition> next_state_transitions_;
};

std::ostream& operator<<(std::ostream& os, DiscoveryStateMachine::State s);

}  // namespace openscreen

#endif
