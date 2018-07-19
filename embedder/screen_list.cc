// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "embedder/screen_list.h"

#include <algorithm>

namespace openscreen {

ScreenList::ScreenList() = default;
ScreenList::~ScreenList() = default;

void ScreenList::OnScreenAdded(const ScreenInfo& info) {
  screens_.emplace_back(info);
}

void ScreenList::OnScreenChanged(const ScreenInfo& info) {
  auto existing_info = std::find_if(
      screens_.begin(), screens_.end(),
      [&info](const ScreenInfo& x) { return x.screen_id == info.screen_id; });
  if (existing_info == screens_.end()) {
    return;
  }
  *existing_info = info;
}

void ScreenList::OnScreenRemoved(const ScreenInfo& info) {
  screens_.erase(std::remove(screens_.begin(), screens_.end(), info),
                 screens_.end());
}

void ScreenList::OnAllScreensRemoved() {
  screens_.clear();
}

}  // namespace openscreen
