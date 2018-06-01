// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "embedder/screen_listener_impl.h"

#include "platform/api/logging.h"

namespace openscreen {
namespace {

using State = DiscoveryStateMachine::State;
using StateTransition = DiscoveryStateMachine::StateTransition;

ScreenListenerState MapInternalStateToExternal(State s) {
  switch (s) {
    case State::kStopped:
      return ScreenListenerState::kStopped;
    case State::kStarting:
      return ScreenListenerState::kStarting;
    case State::kRunning:
      return ScreenListenerState::kRunning;
    case State::kStopping:
      return ScreenListenerState::kStopping;
    case State::kSearchingFromRunning:
      return ScreenListenerState::kSearching;
    case State::kSearchingFromSuspended:
      return ScreenListenerState::kSearching;
    case State::kSuspended:
      return ScreenListenerState::kSuspended;
  }
  DCHECK(false) << "not reached";
  return ScreenListenerState::kStopped;
}

State MapExternalStateToInternal(ScreenListenerState next, State current) {
  switch (next) {
    case ScreenListenerState::kStopped:
      return State::kStopped;
    case ScreenListenerState::kStarting:
      return State::kStarting;
    case ScreenListenerState::kRunning:
      return State::kRunning;
    case ScreenListenerState::kStopping:
      return State::kStopping;
    case ScreenListenerState::kSearching:
      if (current == State::kRunning) {
        return State::kSearchingFromRunning;
      } else if (current == State::kSuspended) {
        return State::kSearchingFromSuspended;
      } else {
        DCHECK(false) << "not reached";
      }
    case ScreenListenerState::kSuspended:
      return State::kSuspended;
  }
  DCHECK(false) << "not reached";
  return State::kStopped;
}

void MaybeNotifyObserver(StateTransition transition,
                         ScreenListenerObserver* observer) {
  if (!observer) {
    return;
  }

  switch (transition) {
    case StateTransition::kStart:
    case StateTransition::kResume:
      observer->OnStarted();
      break;
    case StateTransition::kStop:
      observer->OnStopped();
      break;
    case StateTransition::kSuspend:
    case StateTransition::kStartSuspended:
      observer->OnSuspended();
      break;
    case StateTransition::kSearchNow:
      observer->OnSearching();
      break;
    default:
      break;
  }
}

}  // namespace

ScreenListenerImpl::ScreenListenerImpl(const ScreenList* screen_list)
    : screen_list_(screen_list) {}
ScreenListenerImpl::~ScreenListenerImpl() = default;

std::vector<ScreenListenerImpl::StateTransition>
ScreenListenerImpl::TakeNewStateTransitions() {
  return state_machine_.TakeNewStateTransitions();
}

void ScreenListenerImpl::SetState(ScreenListenerState state) {
  MaybeNotifyObserver(state_machine_.SetState(MapExternalStateToInternal(
                          state, state_machine_.state())),
                      observer_);
}

void ScreenListenerImpl::OnScreenAdded(const ScreenInfo& info) {
  if (observer_) {
    observer_->OnScreenAdded(info);
  }
}

void ScreenListenerImpl::OnScreenChanged(const ScreenInfo& info) {
  if (observer_) {
    observer_->OnScreenChanged(info);
  }
}

void ScreenListenerImpl::OnScreenRemoved(const ScreenInfo& info) {
  if (observer_) {
    observer_->OnScreenRemoved(info);
  }
}

void ScreenListenerImpl::OnAllScreensRemoved() {
  if (observer_) {
    observer_->OnAllScreensRemoved();
  }
}

void ScreenListenerImpl::OnError(ScreenListenerErrorInfo error) {
  last_error_ = error;
  if (observer_) {
    observer_->OnError(error);
  }
}

void ScreenListenerImpl::OnMetrics(ScreenListenerMetrics metrics) {
  if (observer_) {
    observer_->OnMetrics(metrics);
  }
}

bool ScreenListenerImpl::Start() {
  return state_machine_.Start();
}

bool ScreenListenerImpl::StartAndSuspend() {
  return state_machine_.StartAndSuspend();
}

bool ScreenListenerImpl::Stop() {
  return state_machine_.Stop();
}

bool ScreenListenerImpl::Suspend() {
  return state_machine_.Suspend();
}

bool ScreenListenerImpl::Resume() {
  return state_machine_.Resume();
}

bool ScreenListenerImpl::SearchNow() {
  return state_machine_.SearchNow();
}

ScreenListenerState ScreenListenerImpl::state() const {
  return MapInternalStateToExternal(state_machine_.state());
}

const std::vector<ScreenInfo>& ScreenListenerImpl::GetScreens() const {
  return screen_list_->GetScreens();
}

}  // namespace openscreen
