// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_SCREEN_LISTENER_IMPL_H_
#define API_IMPL_SCREEN_LISTENER_IMPL_H_

#include "api/public/screen_info.h"
#include "api/public/screen_listener.h"
#include "api/impl/screen_list.h"

namespace openscreen {

class ScreenListenerImpl final : public ScreenListener {
 public:
  class Delegate {
  public:
    Delegate();
    virtual ~Delegate();

    void SetListenerImpl(ScreenListenerImpl* listener);

    virtual void ListenerStart() = 0;
    virtual void ListenerStartAndSuspend() = 0;
    virtual void ListenerStop() = 0;
    virtual void ListenerSuspend() = 0;
    virtual void ListenerResume() = 0;
    virtual void ListenerSearchNow(ScreenListenerState from) = 0;

  protected:
    ScreenListenerImpl* listener_ = nullptr;
  };

  explicit ScreenListenerImpl(Delegate* delegate);
  ~ScreenListenerImpl() override;

  // Called by |delegate_| to transition the state machine (except kStarting and
  // kStopping which are done automatically).
  void SetState(ScreenListenerState state);

  // Called by |delegate_| when there are updates to the available screens.
  void OnScreenAdded(const ScreenInfo& info);
  void OnScreenChanged(const ScreenInfo& info);
  void OnScreenRemoved(const ScreenInfo& info);
  void OnAllScreensRemoved();

  // Called by |delegate_| when an internal error occurs.
  void OnError(ScreenListenerErrorInfo error);

  // ScreenListener overrides.
  bool Start() override;
  bool StartAndSuspend() override;
  bool Stop() override;
  bool Suspend() override;
  bool Resume() override;
  bool SearchNow() override;

  const std::vector<ScreenInfo>& GetScreens() const override;

 private:
  Delegate* const delegate_;
  ScreenList screen_list_;
};

}  // namespace openscreen

#endif
