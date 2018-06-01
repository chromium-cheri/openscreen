// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "embedder/discovery_state_machine.h"

#include "platform/api/logging.h"

namespace openscreen {
namespace {

using State = DiscoveryStateMachine::State;
using StateTransition = DiscoveryStateMachine::StateTransition;

#if DCHECK_IS_ON()
bool IsTransitionValid(State s1, State s2) {
  switch (s1) {
    case State::kStopped:
      return s2 == State::kStarting || s2 == State::kStopping;
    case State::kStarting:
      return s2 == State::kRunning || s2 == State::kStopping ||
             s2 == State::kSuspended;
    case State::kRunning:
      return s2 == State::kSuspended || s2 == State::kSearchingFromRunning ||
             s2 == State::kStopping;
    case State::kStopping:
      return s2 == State::kStopped;
    case State::kSearchingFromRunning:
      return s2 == State::kRunning || s2 == State::kSuspended ||
             s2 == State::kStopping;
    case State::kSearchingFromSuspended:
      return s2 == State::kRunning || s2 == State::kSuspended ||
             s2 == State::kStopping;
    case State::kSuspended:
      return s2 == State::kRunning || s2 == State::kSearchingFromSuspended ||
             s2 == State::kStopping;
  }
  return false;
}
#endif

StateTransition ComputeTransition(State from, State to) {
  switch (from) {
    case State::kStarting:
      if (to == State::kRunning) {
        return StateTransition::kStart;
      } else if (to == State::kSuspended) {
        return StateTransition::kStartSuspended;
      }
      break;
    case State::kRunning:
      if (to == State::kSuspended) {
        return StateTransition::kSuspend;
      } else if (to == State::kSearchingFromRunning) {
        return StateTransition::kSearchNow;
      }
      break;
    case State::kStopping:
      if (to == State::kStopped) {
        return StateTransition::kStop;
      }
      break;
    case State::kSearchingFromRunning:
      if (to == State::kSuspended) {
        return StateTransition::kSuspend;
      }
      break;
    case State::kSearchingFromSuspended:
      if (to == State::kRunning) {
        return StateTransition::kResume;
      }
      break;
    case State::kSuspended:
      if (to == State::kRunning) {
        return StateTransition::kResume;
      } else if (to == State::kSearchingFromSuspended) {
        return StateTransition::kSearchNow;
      }
      break;
    default:
      break;
  }
  return StateTransition::kNone;
}

}  // namespace

DiscoveryStateMachine::DiscoveryStateMachine() = default;
DiscoveryStateMachine::~DiscoveryStateMachine() = default;

std::vector<StateTransition> DiscoveryStateMachine::TakeNewStateTransitions() {
  return std::move(next_state_transitions_);
}

StateTransition DiscoveryStateMachine::SetState(State state) {
  DCHECK(IsTransitionValid(state_, state)) << state_ << " -> " << state;
  const auto transition = ComputeTransition(state_, state);
  state_ = state;
  return transition;
}

bool DiscoveryStateMachine::Start() {
  if (state_after_transitions_ != State::kStopped) {
    return false;
  }
  next_state_transitions_.push_back(StateTransition::kStart);
  state_after_transitions_ = State::kRunning;
  return true;
}

bool DiscoveryStateMachine::StartAndSuspend() {
  if (state_after_transitions_ != State::kStopped) {
    return false;
  }
  next_state_transitions_.push_back(StateTransition::kStartSuspended);
  state_after_transitions_ = State::kSuspended;
  return true;
}

bool DiscoveryStateMachine::Stop() {
  if (state_after_transitions_ == State::kStopped ||
      state_after_transitions_ == State::kStopping) {
    return false;
  }
  next_state_transitions_.push_back(StateTransition::kStop);
  state_after_transitions_ = State::kStopped;
  return true;
}

bool DiscoveryStateMachine::Suspend() {
  if (state_after_transitions_ != State::kRunning &&
      state_after_transitions_ != State::kSearchingFromRunning) {
    return false;
  }
  next_state_transitions_.push_back(StateTransition::kSuspend);
  state_after_transitions_ = State::kSuspended;
  return true;
}

bool DiscoveryStateMachine::Resume() {
  if (state_after_transitions_ != State::kSuspended &&
      state_after_transitions_ != State::kSearchingFromSuspended) {
    return false;
  }
  next_state_transitions_.push_back(StateTransition::kResume);
  state_after_transitions_ = State::kRunning;
  return true;
}

bool DiscoveryStateMachine::SearchNow() {
  if (state_after_transitions_ != State::kRunning &&
      state_after_transitions_ != State::kSuspended) {
    return false;
  }
  next_state_transitions_.push_back(StateTransition::kSearchNow);
  state_after_transitions_ = state_after_transitions_ == State::kRunning
                                 ? State::kSearchingFromRunning
                                 : State::kSearchingFromSuspended;
  return true;
}

std::ostream& operator<<(std::ostream& os, State s) {
  switch (s) {
    case State::kStopped:
      os << "STOPPED";
      break;
    case State::kStarting:
      os << "STARTING";
      break;
    case State::kRunning:
      os << "RUNNING";
      break;
    case State::kStopping:
      os << "STOPPING";
      break;
    case State::kSearchingFromRunning:
      os << "SEARCHING-FROM-RUNNING";
      break;
    case State::kSearchingFromSuspended:
      os << "SEARCHING-FROM-SUSPENDED";
      break;
    case State::kSuspended:
      os << "SUSPENDED";
      break;
  }
  return os;
}

}  // namespace openscreen
