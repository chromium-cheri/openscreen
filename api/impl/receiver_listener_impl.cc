// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/receiver_listener_impl.h"

#include "platform/api/logging.h"

namespace openscreen {
namespace {

#if OSP_DCHECK_IS_ON()
bool IsTransitionValid(ReceiverListener::State from,
                       ReceiverListener::State to) {
  switch (from) {
    case ReceiverListener::State::kStopped:
      return to == ReceiverListener::State::kStarting ||
             to == ReceiverListener::State::kStopping;
    case ReceiverListener::State::kStarting:
      return to == ReceiverListener::State::kRunning ||
             to == ReceiverListener::State::kStopping ||
             to == ReceiverListener::State::kSuspended;
    case ReceiverListener::State::kRunning:
      return to == ReceiverListener::State::kSuspended ||
             to == ReceiverListener::State::kSearching ||
             to == ReceiverListener::State::kStopping;
    case ReceiverListener::State::kStopping:
      return to == ReceiverListener::State::kStopped;
    case ReceiverListener::State::kSearching:
      return to == ReceiverListener::State::kRunning ||
             to == ReceiverListener::State::kSuspended ||
             to == ReceiverListener::State::kStopping;
    case ReceiverListener::State::kSuspended:
      return to == ReceiverListener::State::kRunning ||
             to == ReceiverListener::State::kSearching ||
             to == ReceiverListener::State::kStopping;
    default:
      OSP_DCHECK(false) << "unknown ReceiverListener::State value: "
                        << static_cast<int>(from);
      break;
  }
  return false;
}
#endif

}  // namespace

ReceiverListenerImpl::Delegate::Delegate() = default;
ReceiverListenerImpl::Delegate::~Delegate() = default;

void ReceiverListenerImpl::Delegate::SetListenerImpl(
    ReceiverListenerImpl* listener) {
  OSP_DCHECK(!listener_);
  listener_ = listener;
}

ReceiverListenerImpl::ReceiverListenerImpl(Observer* observer,
                                           Delegate* delegate)
    : ReceiverListener(observer), delegate_(delegate) {
  delegate_->SetListenerImpl(this);
}

ReceiverListenerImpl::~ReceiverListenerImpl() = default;

void ReceiverListenerImpl::OnReceiverAdded(const ReceiverInfo& info) {
  receiver_list_.OnReceiverAdded(info);
  if (observer_)
    observer_->OnReceiverAdded(info);
}

void ReceiverListenerImpl::OnReceiverChanged(const ReceiverInfo& info) {
  const auto any_changed = receiver_list_.OnReceiverChanged(info);
  if (any_changed && observer_)
    observer_->OnReceiverChanged(info);
}

void ReceiverListenerImpl::OnReceiverRemoved(const ReceiverInfo& info) {
  const auto any_removed = receiver_list_.OnReceiverRemoved(info);
  if (any_removed && observer_)
    observer_->OnReceiverRemoved(info);
}

void ReceiverListenerImpl::OnAllReceiversRemoved() {
  const auto any_removed = receiver_list_.OnAllReceiversRemoved();
  if (any_removed && observer_)
    observer_->OnAllReceiversRemoved();
}

void ReceiverListenerImpl::OnError(ReceiverListenerError error) {
  last_error_ = error;
  if (observer_)
    observer_->OnError(error);
}

bool ReceiverListenerImpl::Start() {
  if (state_ != State::kStopped)
    return false;
  state_ = State::kStarting;
  delegate_->StartListener();
  return true;
}

bool ReceiverListenerImpl::StartAndSuspend() {
  if (state_ != State::kStopped)
    return false;
  state_ = State::kStarting;
  delegate_->StartAndSuspendListener();
  return true;
}

bool ReceiverListenerImpl::Stop() {
  if (state_ == State::kStopped || state_ == State::kStopping)
    return false;

  state_ = State::kStopping;
  delegate_->StopListener();
  return true;
}

bool ReceiverListenerImpl::Suspend() {
  if (state_ != State::kRunning && state_ != State::kSearching &&
      state_ != State::kStarting) {
    return false;
  }
  delegate_->SuspendListener();
  return true;
}

bool ReceiverListenerImpl::Resume() {
  if (state_ != State::kSuspended && state_ != State::kSearching)
    return false;

  delegate_->ResumeListener();
  return true;
}

bool ReceiverListenerImpl::SearchNow() {
  if (state_ != State::kRunning && state_ != State::kSuspended)
    return false;

  delegate_->SearchNow(state_);
  return true;
}

const std::vector<ReceiverInfo>& ReceiverListenerImpl::GetReceivers() const {
  return receiver_list_.receivers();
}

void ReceiverListenerImpl::SetState(State state) {
  OSP_DCHECK(IsTransitionValid(state_, state));
  state_ = state;
  if (observer_)
    MaybeNotifyObserver();
}

void ReceiverListenerImpl::MaybeNotifyObserver() {
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
    case State::kSearching:
      observer_->OnSearching();
      break;
    default:
      break;
  }
}

}  // namespace openscreen
