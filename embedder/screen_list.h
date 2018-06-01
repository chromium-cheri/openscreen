// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EMBEDDER_SCREEN_LIST_H_
#define EMBEDDER_SCREEN_LIST_H_

#include <vector>

#include "api/screen_info.h"

namespace openscreen {

class ScreenList {
 public:
  ScreenList();
  ~ScreenList();
  ScreenList(ScreenList&&) = delete;
  ScreenList& operator=(ScreenList&&) = delete;

  void OnScreenAdded(const ScreenInfo& info);
  void OnScreenChanged(const ScreenInfo& info);
  void OnScreenRemoved(const ScreenInfo& info);
  void OnAllScreensRemoved();

  const std::vector<ScreenInfo>& GetScreens() const { return screens_; }

 private:
  std::vector<ScreenInfo> screens_;
};

}  // namespace openscreen

#endif
