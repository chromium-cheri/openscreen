// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_POSIX_EVENT_WAITER_H_
#define PLATFORM_POSIX_EVENT_WAITER_H_

#include <unistd.h>

#include <atomic>
#include <mutex>  // NOLINT

#include "platform/api/event_waiter.h"
#include "platform/posix/udp_socket.h"

namespace openscreen {
namespace platform {

// The implementation of the above interface to use posix sockets.
class SocketHandlerPosix : public EventWaiter::SocketHandler {
 public:
  SocketHandlerPosix();
  ~SocketHandlerPosix();
  void Watch(const UdpSocket& socket) override;
  bool IsChanged(const UdpSocket& socket) const override;
  void Clear() override;

  // Converts a timespan to a struct timeval.
  static struct timeval ToTimeval(Clock::duration timeout);

  // Watches a given set of SocketHandlerPosix for changes.
  static Error WatchForChanges(SocketHandlerPosix* read_handles,
                               SocketHandlerPosix* write_handles,
                               Clock::duration timeout);

  // The underlying fd_set used to watch sockets.
  fd_set set;

  // Max file handle in the above set.
  int max_fd;
};

class EventWaiterPosix : public EventWaiter {
 public:
  EventWaiterPosix();
  ~EventWaiterPosix();
  Error WaitForEvents(Clock::duration timeout,
                      SocketHandler* reads,
                      SocketHandler* writes);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_POSIX_EVENT_WAITER_H_
