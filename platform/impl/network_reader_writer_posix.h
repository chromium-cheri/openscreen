// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_NETWORK_READER_WRITER_POSIX_H_
#define PLATFORM_IMPL_NETWORK_READER_WRITER_POSIX_H_

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "platform/base/macros.h"

namespace openscreen {
namespace platform {

class NetworkReaderWriterPosix {
 public:
  // A provider is a class which performs networking operations at regular
  // intervals.
  class Provider {
   public:
    virtual ~Provider() = default;

    // Performs the Provider's networking tasks. This task is expected to
    // require a nontrivial amount of time.
    virtual void PerformNetworkingOperations() = 0;
  };

  NetworkReaderWriterPosix() = default;
  virtual ~NetworkReaderWriterPosix() = default;

  // Begins watching a new provider for networking operations. NOTE: There may
  // be a delay before this provider's operations begin to execute.
  virtual void RegisterProvider(Provider* provider);

  // Stops watching the given provider for networking operations, blocking
  // until the operation can proceed safely.
  virtual void DeregisterProvider(Provider* provider);

  // Runs the Wait function in a loop until the below RequestStopSoon function
  // is called.
  void RunUntilStopped();

  // Signals for the RunUntilStopped loop to cease running.
  void RequestStopSoon();

  OSP_DISALLOW_COPY_AND_ASSIGN(NetworkReaderWriterPosix);

 private:
  // Set of provider operations that may be performed by outside callers.
  enum class ProviderOperation { kAddProvider, kRemoveProvider };

  using ProviderOperationInfo = std::pair<Provider*, ProviderOperation>;

  // Performs a single iteration of the wait loop.
  void Wait();

  // Determines whether provider_changes has any waiting operations on provider.
  // NOTE: The caller is expected to already have the lock on mutex_;
  bool IsWaitingOnProviderChange(Provider* provider);

  // Represents whether this instance is currently "running".
  std::atomic_bool is_running_{false};

  // Guards against concurrent access to providers_changes_.
  std::mutex mutex_;

  // Set of all operations to perform on the providers_ vector when a valid time
  // to do so arises.
  std::vector<ProviderOperationInfo> providers_changes_ GUARDED_BY(mutex_);

  // Providers currently being used by this object.
  std::vector<Provider*> providers_;

  // Allows StopProviding operations to block until completed.
  std::condition_variable stop_providing_block_;

  // Allows for blocking of the thread when there are no watched Providers. This
  // prevents the CPU from wasting cycles when no useful work can be done.
  std::condition_variable empty_providers_block_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_NETWORK_READER_WRITER_POSIX_H_
