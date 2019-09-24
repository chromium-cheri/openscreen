// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_NETWORK_READER_WRITER_H_
#define PLATFORM_IMPL_NETWORK_READER_WRITER_H_

#include "platform/impl/network_reader_writer_posix.h"

#include <algorithm>

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {

void NetworkReaderWriterPosix::StartProviding(Provider* provider) {
  std::lock_guard<std::mutex> lock(mutex_);
  providers_changes_.emplace_back(provider, ProviderOperation::kAddProvider);
  empty_providers_block_.notify_all();
}

void NetworkReaderWriterPosix::StopProviding(Provider* provider) {
  std::lock_guard<std::mutex> lock(mutex_);
  providers_changes_.emplace_back(provider, ProviderOperation::kRemoveProvider);
}

void NetworkReaderWriterPosix::Wait() {
  // Notify any waiting StopProviding calls that the provider is no longer being
  // used, so it's safe to proceed.
  stop_providing_block_.notify_all();

  // Apply any waiting changes to providers_.
  {
    std::unique_lock<std::mutex> lock(mutex_);
    if (providers_changes_.empty() && providers_.empty()) {
      empty_providers_block_.wait(lock);
    }

    if (!providers_changes_.empty()) {
      for (auto operation : providers_changes_) {
        ProviderOperation change_type = std::get<1>(operation);
        Provider* provider = std::get<0>(operation);
        const auto it =
            std::find(providers_.begin(), providers_.end(), provider);
        if (change_type == ProviderOperation::kAddProvider) {
          if (it == providers_.end()) {
            providers_.push_back(provider);
          }
        } else {
          if (it != providers_.end()) {
            providers_.erase(it);
          }
        }
      }
    }
  }

  // Perform the operation defined by each provider.
  for (Provider* provider : providers_) {
    provider->PerformNetworkingOperation();
  }
}

void NetworkReaderWriterPosix::RunUntilStopped() {
  const bool was_running = is_running_.exchange(true);
  OSP_CHECK(!was_running);

  while (is_running_) {
    Wait();
  }
}

void NetworkReaderWriterPosix::RequestStopSoon() {
  is_running_.store(false);
}

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_NETWORK_READER_WRITER_H_
