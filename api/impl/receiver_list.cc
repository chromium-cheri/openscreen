// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/receiver_list.h"

#include <algorithm>

namespace openscreen {

ReceiverList::ReceiverList() = default;
ReceiverList::~ReceiverList() = default;

void ReceiverList::OnReceiverAdded(const ReceiverInfo& info) {
  receivers_.emplace_back(info);
}

bool ReceiverList::OnReceiverChanged(const ReceiverInfo& info) {
  auto existing_info = std::find_if(receivers_.begin(), receivers_.end(),
                                    [&info](const ReceiverInfo& x) {
                                      return x.receiver_id == info.receiver_id;
                                    });
  if (existing_info == receivers_.end())
    return false;

  *existing_info = info;
  return true;
}

bool ReceiverList::OnReceiverRemoved(const ReceiverInfo& info) {
  const auto it = std::remove(receivers_.begin(), receivers_.end(), info);
  if (it == receivers_.end())
    return false;

  receivers_.erase(it, receivers_.end());
  return true;
}

bool ReceiverList::OnAllReceiversRemoved() {
  const auto empty = receivers_.empty();
  receivers_.clear();
  return !empty;
}

}  // namespace openscreen
