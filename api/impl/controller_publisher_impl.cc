// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/controller_publisher_impl.h"

#include "platform/api/logging.h"

namespace openscreen {
namespace {

#if OSP_DCHECK_IS_ON()
bool IsTransitionValid(ControllerPublisher::State from,
                       ControllerPublisher::State to) {
  using State = ControllerPublisher::State;
  switch (from) {
    case State::kStopped:
      return to == State::kStarting || to == State::kStopping;
    case State::kStarting:
      return to == State::kRunning || to == State::kStopping ||
             to == State::kSuspended;
    case State::kRunning:
      return to == State::kSuspended || to == State::kStopping;
    case State::kStopping:
      return to == State::kStopped;
    case State::kSuspended:
      return to == State::kRunning || to == State::kStopping;
    default:
      OSP_DCHECK(false) << "unknown State value: " << static_cast<int>(from);
      break;
  }
  return false;
}
#endif

}  // namespace

ControllerPublisherImpl::Delegate::Delegate() = default;
ControllerPublisherImpl::Delegate::~Delegate() = default;

void ControllerPublisherImpl::Delegate::SetPublisherImpl(
    ControllerPublisherImpl* publisher) {
  OSP_DCHECK(!publisher_);
  publisher_ = publisher;
}

ControllerPublisherImpl::ControllerPublisherImpl(Observer* observer,
                                                 Delegate* delegate)
    : ControllerPublisher(observer), delegate_(delegate) {
  delegate_->SetPublisherImpl(this);
}

ControllerPublisherImpl::~ControllerPublisherImpl() = default;

bool ControllerPublisherImpl::Start() {
  if (state_ != State::kStopped)
    return false;
  state_ = State::kStarting;
  delegate_->StartPublisher();
  return true;
}
bool ControllerPublisherImpl::StartAndSuspend() {
  if (state_ != State::kStopped)
    return false;
  state_ = State::kStarting;
  delegate_->StartAndSuspendPublisher();
  return true;
}
bool ControllerPublisherImpl::Stop() {
  if (state_ == State::kStopped || state_ == State::kStopping)
    return false;

  state_ = State::kStopping;
  delegate_->StopPublisher();
  return true;
}
bool ControllerPublisherImpl::Suspend() {
  if (state_ != State::kRunning && state_ != State::kStarting)
    return false;

  delegate_->SuspendPublisher();
  return true;
}
bool ControllerPublisherImpl::Resume() {
  if (state_ != State::kSuspended)
    return false;

  delegate_->ResumePublisher();
  return true;
}

void ControllerPublisherImpl::SetState(State state) {
  OSP_DCHECK(IsTransitionValid(state_, state));
  state_ = state;
  if (observer_)
    MaybeNotifyObserver();
}

void ControllerPublisherImpl::MaybeNotifyObserver() {
  OSP_DCHECK(observer_);
  switch (state_) {
    case State::kRunning:
      observer_->OnStarted();
      break;
    case State::kStopped:
      observer_->OnStopped();
      break;
    case State::kSuspended:
      observer_->OnSuspended();
      break;
    default:
      break;
  }
}

}  // namespace openscreen
