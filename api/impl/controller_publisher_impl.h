// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_CONTROLLER_PUBLISHER_IMPL_H_
#define API_IMPL_CONTROLLER_PUBLISHER_IMPL_H_

#include "api/public/controller_publisher.h"
#include "base/macros.h"
#include "base/with_destruction_callback.h"

namespace openscreen {

class ControllerPublisherImpl final : public ControllerPublisher,
                                      public WithDestructionCallback {
 public:
  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();

    void SetPublisherImpl(ControllerPublisherImpl* publisher);

    virtual void StartPublisher() = 0;
    virtual void StartAndSuspendPublisher() = 0;
    virtual void StopPublisher() = 0;
    virtual void SuspendPublisher() = 0;
    virtual void ResumePublisher() = 0;

   protected:
    void SetState(State state) { publisher_->SetState(state); }

    ControllerPublisherImpl* publisher_ = nullptr;
  };

  // |observer| is optional.  If it is provided, it will receive appropriate
  // notifications about this ControllerPublisher.  |delegate| is required and
  // is used to implement state transitions.
  ControllerPublisherImpl(Observer* observer, Delegate* delegate);
  ~ControllerPublisherImpl() override;

  // ControllerPublisher overrides.
  bool Start() override;
  bool StartAndSuspend() override;
  bool Stop() override;
  bool Suspend() override;
  bool Resume() override;

 private:
  // Called by |delegate_| to transition the state machine (except kStarting and
  // kStopping which are done automatically).
  void SetState(State state);

  // Notifies |observer_| if the transition to |state_| is one that is watched
  // by the observer interface.
  void MaybeNotifyObserver();

  Delegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(ControllerPublisherImpl);
};

}  // namespace openscreen

#endif  // API_IMPL_CONTROLLER_PUBLISHER_IMPL_H_
