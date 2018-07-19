// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_SCREEN_LISTENER_IMPL_H_
#define EMBEDDER_SCREEN_LISTENER_IMPL_H_

#include <vector>

#include "embedder/api/screen_listener.h"
#include "embedder/discovery_state_machine.h"
#include "embedder/screen_list.h"

namespace openscreen {

class ScreenListenerImpl final : public ScreenListener {
 public:
  using StateTransition = DiscoveryStateMachine::StateTransition;

  explicit ScreenListenerImpl(const ScreenList* screen_list);
  ~ScreenListenerImpl() override;

  std::vector<StateTransition> TakeNewStateTransitions();
  void SetState(ScreenListenerState state);

  void OnScreenAdded(const ScreenInfo& info);
  void OnScreenChanged(const ScreenInfo& info);
  void OnScreenRemoved(const ScreenInfo& info);
  void OnAllScreensRemoved();
  void OnError(ScreenListenerError error);
  void OnMetrics(ScreenListenerMetrics metrics);

  // ScreenListener overrides.
  bool Start() override;
  bool StartAndSuspend() override;
  bool Stop() override;
  bool Suspend() override;
  bool Resume() override;
  bool SearchNow() override;

  ScreenListenerState state() const override;
  ScreenListenerError GetLastError() const override;
  void SetObserver(ScreenListenerObserver* observer) override;
  const std::vector<ScreenInfo>& GetScreens() const override;

 private:
  ScreenListenerError last_error_;
  DiscoveryStateMachine state_machine_;
  const ScreenList* const screen_list_;
  ScreenListenerObserver* observer_ = nullptr;
};

}  // namespace openscreen

#endif
