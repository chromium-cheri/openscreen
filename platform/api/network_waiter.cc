// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/network_waiter.h"

#include <algorithm>
#include <atomic>

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {

void NetworkWaiter::Subscribe(Subscriber* subscriber) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (std::find(subscribers_.begin(), subscribers_.end(), subscriber) !=
      subscribers_.end()) {
    return;
  }

  subscribers_.push_back(subscriber);
}

void NetworkWaiter::Unsubscribe(Subscriber* subscriber) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto iterator =
      std::find(subscribers_.begin(), subscribers_.end(), subscriber);
  if (iterator == subscribers_.end()) {
    return;
  }

  subscribers_.erase(iterator);
}

std::vector<int> NetworkWaiter::GetFds(
    std::map<int, Subscriber*>* fd_mappings) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<int> fds;
  for (Subscriber* subscriber : subscribers_) {
    std::vector<int> subscriber_fds = subscriber->GetFds();
    fds.reserve(subscriber_fds.size());
    for (int fd : subscriber_fds) {
      fds.push_back(fd);
      auto result = fd_mappings->emplace(fd, subscriber);
      OSP_DCHECK(result.second)
          << "The same fd cannot be provided from multiple subscribers";
    }
  }

  return fds;
}

void NetworkWaiter::ProcessReadyFds(
    const std::map<int, Subscriber*> fd_mappings,
    const std::vector<int>& fds) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (int fd : fds) {
    auto iterator = fd_mappings.find(fd);
    OSP_DCHECK(iterator != fd_mappings.end());
    if (std::find(subscribers_.begin(), subscribers_.end(), iterator->second) ==
        subscribers_.end()) {
      continue;
    }

    iterator->second->ProcessReadyFd(fd);
  }
}

Error NetworkWaiter::ProcessFds(const Clock::duration& timeout) {
  std::map<int, Subscriber*> fd_mappings;
  std::vector<int> fds = GetFds(&fd_mappings);
  ErrorOr<std::vector<int>> changed_fds = AwaitSocketsReadable(fds, timeout);
  if (changed_fds.is_error()) {
    return changed_fds.error();
  }
  ProcessReadyFds(fd_mappings, changed_fds.value());
  return Error::None();
}

}  // namespace platform
}  // namespace openscreen
