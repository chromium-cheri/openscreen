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

void NetworkReaderWriterPosix::RegisterProvider(Provider* provider) {
  std::lock_guard<std::mutex> lock(mutex_);
  providers_changes_.emplace_back(provider, ProviderOperation::kAddProvider);
  empty_providers_block_.notify_all();
}

void NetworkReaderWriterPosix::DeregiserProvider(Provider* provider) {
  std::unique_lock<std::mutex> lock(mutex_);
  providers_changes_.emplace_back(provider, ProviderOperation::kRemoveProvider);
  stop_providing_block_.wait(lock, [this, provider]() {
    return this->IsWaitingOnProviderChange(provider);
  });
}

bool NetworkReaderWriterPosix::IsWaitingOnProviderChange(Provider* provider) {
  return std::find_if(providers_changes_.begin(), providers_changes_.end(),
                      [provider](ProviderOperationInfo operation) {
                        return provider == operation.first;
                      }) != providers_changes_.end();
}

void NetworkReaderWriterPosix::Wait() {
  // Apply any waiting changes to providers_.
  {
    std::unique_lock<std::mutex> lock(mutex_);
    if (providers_changes_.empty() && providers_.empty()) {
      empty_providers_block_.wait(
          lock, [this]() { return this->providers_changes_.empty(); });
    }

    if (!providers_changes_.empty()) {
      for (const ProviderOperationInfo& operation : providers_changes_) {
        Provider* provider = operation.first;
        const auto it =
            std::find(providers_.begin(), providers_.end(), provider);
        switch (operation.second) {
          case ProviderOperation::kAddProvider:
            if (it == providers_.end()) {
              providers_.push_back(provider);
            }
            break;
          case ProviderOperation::kRemoveProvider:
            if (it != providers_.end()) {
              providers_.erase(it);
            }
            break;
        }
      }
    }

    // Notify any waiting StopProviding calls that the provider is no longer
    // being used, so it's safe to proceed.
    stop_providing_block_.notify_all();
  }

  // Perform the operation defined by each provider.
  for (Provider* provider : providers_) {
    provider->PerformNetworkingOperations();
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
