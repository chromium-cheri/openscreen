// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/screen_listener_impl.h"

#include "platform/api/logging.h"

namespace openscreen {
namespace {

#if DCHECK_IS_ON()
bool IsTransitionValid(ScreenListenerState s1, ScreenListenerState s2) {
  switch (s1) {
    case ScreenListenerState::kStopped:
      return s2 == ScreenListenerState::kStarting ||
             s2 == ScreenListenerState::kStopping;
    case ScreenListenerState::kStarting:
      return s2 == ScreenListenerState::kRunning ||
             s2 == ScreenListenerState::kStopping ||
             s2 == ScreenListenerState::kSuspended;
    case ScreenListenerState::kRunning:
      return s2 == ScreenListenerState::kSuspended ||
             s2 == ScreenListenerState::kSearching ||
             s2 == ScreenListenerState::kStopping;
    case ScreenListenerState::kStopping:
      return s2 == ScreenListenerState::kStopped;
    case ScreenListenerState::kSearching:
      return s2 == ScreenListenerState::kRunning ||
             s2 == ScreenListenerState::kSuspended ||
             s2 == ScreenListenerState::kStopping;
    case ScreenListenerState::kSuspended:
      return s2 == ScreenListenerState::kRunning ||
             s2 == ScreenListenerState::kSearching ||
             s2 == ScreenListenerState::kStopping;
  }
  return false;
}
#endif

void MaybeNotifyObserver(ScreenListenerState from,
                         ScreenListenerState to,
                         ScreenListenerObserver* observer) {
  if (!observer) {
    return;
  }

  switch (to) {
    case ScreenListenerState::kRunning:
      if (from == ScreenListenerState::kStarting ||
          from == ScreenListenerState::kSuspended ||
          from == ScreenListenerState::kSearching) {
        observer->OnStarted();
      }
      break;
    case ScreenListenerState::kStopped:
      observer->OnStopped();
      break;
    case ScreenListenerState::kSuspended:
      if (from == ScreenListenerState::kRunning ||
          from == ScreenListenerState::kSearching) {
        observer->OnSuspended();
      }
      break;
    case ScreenListenerState::kSearching:
      observer->OnSearching();
      break;
    default:
      break;
  }
}

}  // namespace

ScreenListenerImpl::Delegate::Delegate() = default;
ScreenListenerImpl::Delegate::~Delegate() = default;

void ScreenListenerImpl::Delegate::SetListenerImpl(
    ScreenListenerImpl* listener) {
  DCHECK(!listener_);
  listener_ = listener;
}

ScreenListenerImpl::ScreenListenerImpl(Delegate* delegate)
    : delegate_(delegate) {
  delegate_->SetListenerImpl(this);
}

ScreenListenerImpl::~ScreenListenerImpl() = default;

void ScreenListenerImpl::SetState(ScreenListenerState state) {
  DCHECK(IsTransitionValid(state_, state));
  const auto from = state_;
  state_ = state;
  MaybeNotifyObserver(from, state_, observer_);
}

void ScreenListenerImpl::OnScreenAdded(const ScreenInfo& info) {
  screen_list_.OnScreenAdded(info);
  if (observer_) {
    observer_->OnScreenAdded(info);
  }
}

void ScreenListenerImpl::OnScreenChanged(const ScreenInfo& info) {
  screen_list_.OnScreenChanged(info);
  if (observer_) {
    observer_->OnScreenChanged(info);
  }
}

void ScreenListenerImpl::OnScreenRemoved(const ScreenInfo& info) {
  screen_list_.OnScreenRemoved(info);
  if (observer_) {
    observer_->OnScreenRemoved(info);
  }
}

void ScreenListenerImpl::OnAllScreensRemoved() {
  screen_list_.OnAllScreensRemoved();
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

// TODO: Check for legit calls.
bool ScreenListenerImpl::Start() {
  if (state_ != ScreenListenerState::kStopped) {
    return false;
  }
  state_ = ScreenListenerState::kStarting;
  delegate_->ListenerStart();
  return true;
}

bool ScreenListenerImpl::StartAndSuspend() {
  if (state_ != ScreenListenerState::kStopped) {
    return false;
  }
  state_ = ScreenListenerState::kStarting;
  delegate_->ListenerStartAndSuspend();
  return true;
}

bool ScreenListenerImpl::Stop() {
  if (state_ == ScreenListenerState::kStopped ||
      state_ == ScreenListenerState::kStopping) {
    return false;
  }
  state_ = ScreenListenerState::kStopping;
  delegate_->ListenerStop();
  return true;
}

bool ScreenListenerImpl::Suspend() {
  if (state_ != ScreenListenerState::kRunning &&
      state_ != ScreenListenerState::kSearching) {
    return false;
  }
  delegate_->ListenerSuspend();
  return true;
}

bool ScreenListenerImpl::Resume() {
  if (state_ != ScreenListenerState::kSuspended &&
      state_ != ScreenListenerState::kSearching) {
    return false;
  }
  delegate_->ListenerResume();
  return true;
}

bool ScreenListenerImpl::SearchNow() {
  if (state_ != ScreenListenerState::kRunning &&
      state_ != ScreenListenerState::kSuspended) {
    return false;
  }
  delegate_->ListenerSearchNow(state_);
  return true;
}

const std::vector<ScreenInfo>& ScreenListenerImpl::GetScreens() const {
  return screen_list_.GetScreens();
}

}  // namespace openscreen
