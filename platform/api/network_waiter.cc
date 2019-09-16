// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/network_waiter.h"

#include <algorithm>
#include <atomic>

#include "absl/algorithm/container.h"
#include "platform/api/logging.h"

namespace openscreen {
namespace platform {

void NetworkWaiter::Subscribe(Subscriber* subscriber) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (absl::c_find(subscribers_, subscriber) != subscribers_.end()) {
    return;
  }

  subscribers_.push_back(subscriber);
}

void NetworkWaiter::Unsubscribe(Subscriber* subscriber) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto iterator = absl::c_find(subscribers_, subscriber);
  if (iterator == subscribers_.end()) {
    return;
  }

  subscribers_.erase(iterator);
}

std::vector<NetworkWaiter::SocketHandleRef> NetworkWaiter::GetHandles(
    std::map<SocketHandleRef, Subscriber*>* handle_mappings) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<SocketHandleRef> handles;
  for (Subscriber* subscriber : subscribers_) {
    const std::vector<SocketHandleRef> subscriber_handles =
        subscriber->GetHandles();
    handles.reserve(subscriber_handles.size());
    for (const SocketHandleRef& handle : subscriber_handles) {
      handles.push_back(handle);
      const auto result = handle_mappings->emplace(handle, subscriber);
      OSP_DCHECK(result.second)
          << "The same handle cannot be provided from multiple subscribers";
    }
  }

  return handles;
}

void NetworkWaiter::ProcessReadyHandles(
    const std::map<SocketHandleRef, Subscriber*> handle_mappings,
    const std::vector<SocketHandleRef>& handles) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (const SocketHandleRef& handle : handles) {
    auto iterator = handle_mappings.find(handle.get());
    OSP_DCHECK(iterator != handle_mappings.end());
    if (absl::c_find(subscribers_, iterator->second) == subscribers_.end()) {
      continue;
    }

    iterator->second->ProcessReadyHandle(handle);
  }
}

Error NetworkWaiter::ProcessHandles(const Clock::duration& timeout) {
  std::map<SocketHandleRef, Subscriber*> handle_mappings;
  std::vector<SocketHandleRef> handles = GetHandles(&handle_mappings);
  ErrorOr<std::vector<SocketHandleRef>> changed_handles =
      AwaitSocketsReadable(handles, timeout);
  if (changed_handles.is_error()) {
    return changed_handles.error();
  }
  ProcessReadyHandles(handle_mappings, changed_handles.value());
  return Error::None();
}

}  // namespace platform
}  // namespace openscreen
