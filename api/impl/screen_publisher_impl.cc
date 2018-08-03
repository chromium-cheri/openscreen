// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/screen_publisher_impl.h"

#include "platform/api/logging.h"

namespace openscreen {
namespace {

#if DCHECK_IS_ON()
bool IsTransitionValid(ScreenPublisherState from, ScreenPublisherState to) {
  switch (from) {
    case ScreenPublisherState::kStopped:
      return to == ScreenPublisherState::kStarting ||
             to == ScreenPublisherState::kStopping;
    case ScreenPublisherState::kStarting:
      return to == ScreenPublisherState::kRunning ||
             to == ScreenPublisherState::kStopping ||
             to == ScreenPublisherState::kSuspended;
    case ScreenPublisherState::kRunning:
      return to == ScreenPublisherState::kSuspended ||
             to == ScreenPublisherState::kStopping;
    case ScreenPublisherState::kStopping:
      return to == ScreenPublisherState::kStopped;
    case ScreenPublisherState::kSuspended:
      return to == ScreenPublisherState::kRunning ||
             to == ScreenPublisherState::kStopping;
    default:
      DCHECK(false) << "unknown ScreenPublisherState value: "
                    << static_cast<int>(from);
      break;
  }
  return false;
}
#endif

}  // namespace

ScreenPublisherImpl::Delegate::Delegate() = default;
ScreenPublisherImpl::Delegate::~Delegate() = default;

void ScreenPublisherImpl::Delegate::SetPublisherImpl(
    ScreenPublisherImpl* publisher) {
  DCHECK(!publisher_);
  publisher_ = publisher;
}

ScreenPublisherImpl::ScreenPublisherImpl(ScreenPublisherObserver* observer,
                                         Delegate* delegate)
    : ScreenPublisher(observer), delegate_(delegate) {
  delegate_->SetPublisherImpl(this);
}

ScreenPublisherImpl::~ScreenPublisherImpl() = default;

bool ScreenPublisherImpl::Start() {
  if (state_ != ScreenPublisherState::kStopped)
    return false;
  state_ = ScreenPublisherState::kStarting;
  delegate_->StartPublisher();
  return true;
}
bool ScreenPublisherImpl::StartAndSuspend() {
  if (state_ != ScreenPublisherState::kStopped)
    return false;
  state_ = ScreenPublisherState::kStarting;
  delegate_->StartAndSuspendPublisher();
  return true;
}
bool ScreenPublisherImpl::Stop() {
  if (state_ == ScreenPublisherState::kStopped ||
      state_ == ScreenPublisherState::kStopping) {
    return false;
  }
  state_ = ScreenPublisherState::kStopping;
  delegate_->StopPublisher();
  return true;
}
bool ScreenPublisherImpl::Suspend() {
  if (state_ != ScreenPublisherState::kRunning &&
      state_ != ScreenPublisherState::kStarting) {
    return false;
  }
  delegate_->SuspendPublisher();
  return true;
}
bool ScreenPublisherImpl::Resume() {
  if (state_ != ScreenPublisherState::kSuspended) {
    return false;
  }
  delegate_->ResumePublisher();
  return true;
}
void ScreenPublisherImpl::UpdateFriendlyName(const std::string& friendly_name) {
  delegate_->UpdateFriendlyName(friendly_name);
}

void ScreenPublisherImpl::SetState(ScreenPublisherState state) {
  DCHECK(IsTransitionValid(state_, state));
  const auto from = state_;
  state_ = state;
  if (observer_)
    MaybeNotifyObserver(from);
}

void ScreenPublisherImpl::MaybeNotifyObserver(ScreenPublisherState from) {
  DCHECK(observer_);
  switch (state_) {
    case ScreenPublisherState::kRunning:
      if (from == ScreenPublisherState::kStarting ||
          from == ScreenPublisherState::kSuspended) {
        observer_->OnStarted();
      }
      break;
    case ScreenPublisherState::kStopped:
      observer_->OnStopped();
      break;
    case ScreenPublisherState::kSuspended:
      if (from == ScreenPublisherState::kRunning) {
        observer_->OnSuspended();
      }
      break;
    default:
      break;
  }
}

}  // namespace openscreen
