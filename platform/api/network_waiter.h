// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_NETWORK_WAITER_H_
#define PLATFORM_API_NETWORK_WAITER_H_

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "platform/api/socket_handle.h"
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
  using SocketHandleRef = std::reference_wrapper<const SocketHandle>;

  class Subscriber {
   public:
    virtual ~Subscriber() = default;

    // Returns the socket handles which this subscriber would like to wait on.
    virtual std::vector<SocketHandleRef> GetHandles() = 0;

    // Provides a socket handle to the subscriber which has data waiting to be
    // processed.
    virtual void ProcessReadyHandle(SocketHandleRef handle) = 0;
  };

  // Creates a new NetworkWaiter instance.
  static std::unique_ptr<NetworkWaiter> Create();

  virtual ~NetworkWaiter() = default;

  void Subscribe(Subscriber* subscriber);
  void Unsubscribe(Subscriber* subscriber);

 protected:
  // Gets all socket handles to process, checks them for readable data, and
  // handles any changes that have occured.
  Error ProcessHandles(const Clock::duration& timeout);

  // Waits until data is available in one of the provided sockets or the
  // provided timeout has passed - whichever is first. If any sockets have data
  // available, they are returned.
  virtual ErrorOr<std::vector<SocketHandleRef>> AwaitSocketsReadable(
      const std::vector<SocketHandleRef>& socket_fds,
      const Clock::duration& timeout) = 0;

 private:
  // Returns the socket handles to check for this run of the NetworkWaiter.
  // The handle_mappings parameter should be an empty map which will be
  // popualted with mappings from fd to the subscriger it came from.
  std::vector<SocketHandleRef> GetHandles(
      std::map<SocketHandleRef, Subscriber*>* handle_mappings);

  // Call the subscriber associated with each changed handle.
  void ProcessReadyHandles(
      const std::map<SocketHandleRef, Subscriber*> handle_mappings,
      const std::vector<SocketHandleRef>& handles);

  std::vector<Subscriber*> subscribers_;
  std::mutex mutex_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_NETWORK_WAITER_H_
