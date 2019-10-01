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

  // Creates a new NetworkReaderWriterPosix from a variable number of providers.
  // All providers are expected to live for the duration of this object's
  // lifetime.
  NetworkReaderWriterPosix(std::vector<Provider*> providers);

  // Runs the Wait function in a loop until the below RequestStopSoon function
  // is called.
  void RunUntilStopped();

  // Signals for the RunUntilStopped loop to cease running.
  void RequestStopSoon();

  OSP_DISALLOW_COPY_AND_ASSIGN(NetworkReaderWriterPosix);

 private:
  // Performs a single iteration of the wait loop.
  void Wait();

  // Determines whether provider_changes has any waiting operations on provider.
  // NOTE: The caller is expected to already have the lock on mutex_;
  bool IsWaitingOnProviderChange(Provider* provider);

  // Represents whether this instance is currently "running".
  std::atomic_bool is_running_{false};

  // Providers currently being used by this object.
  std::vector<Provider*> providers_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_NETWORK_READER_WRITER_POSIX_H_
