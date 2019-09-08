// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_NETWORK_WAITER_H_
#define PLATFORM_API_NETWORK_WAITER_H_

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "platform/api/time.h"
#include "platform/base/error.h"
#include "platform/base/macros.h"

namespace openscreen {
namespace platform {

// The class responsible for calling platform-level method to watch UDP sockets
// for available read data. Reading from these sockets is handled at a higher
// layer.
class NetworkWaiter {
 public:
  class Subscriber {
   public:
    virtual ~Subscriber() = default;

    // Returns the File Descriptors which this subscriber would like to wait on.
    virtual std::vector<int> GetFds() = 0;

    // Provides a File Descriptor to the subscriber which has data waiting to be
    // processed.
    virtual void ProcessReadyFd(int fd) = 0;
  };

  // Creates a new NetworkWaiter instance.
  static std::unique_ptr<NetworkWaiter> Create();

  virtual ~NetworkWaiter() = default;

  void Subscribe(Subscriber* subscriber);
  void Unsubscribe(Subscriber* subscriber);

 protected:
  // Gets all fds to process, checks them for readable data, and handles any
  // changes that have occured.
  Error ProcessFds(const Clock::duration& timeout);

  // Waits until data is available in one of the provided sockets or the
  // provided timeout has passed - whichever is first. If any sockets have data
  // available, they are returned. Else, an error is returned.
  virtual ErrorOr<std::vector<int>> AwaitSocketsReadable(
      const std::vector<int>& fds,
      const Clock::duration& timeout) = 0;

 private:
  // Returns the file descriptors to check for this run of the NetworkWaiter.
  // The fd_mappings parameter should be an empty map which will be popualted
  // with mappings from fd to the subscriger it came from.
  std::vector<int> GetFds(std::map<int, Subscriber*>* fd_mappings);

  // Call the subscriber associated with each changed fd.
  void ProcessReadyFds(const std::map<int, Subscriber*> fd_mappings,
                       const std::vector<int>& fds);

  std::vector<Subscriber*> subscribers_;
  std::mutex mutex_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_NETWORK_WAITER_H_
