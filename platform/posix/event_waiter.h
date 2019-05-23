// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_POSIX_EVENT_WAITER_H_
#define PLATFORM_POSIX_EVENT_WAITER_H_

#include <unistd.h>

#include <atomic>
#include <mutex>  // NOLINT

#include "platform/api/event_waiter.h"
#include "platform/posix/socket.h"

namespace openscreen {
namespace platform {

// A interface to hide all socket-level manipulation done in this class.
// NOTE: This interface is required for UTs.
class SocketHandler {
 public:
  // Watches the selected file handle.
  virtual void Watch(int fd, bool is_real_socket = true) = 0;

  // Determines if the given handle has changed.
  virtual bool IsChanged(int fd) = 0;
};

// The implementation of the above interface to use posix sockets.
class SocketHandlerPosix : public SocketHandler {
 public:
  SocketHandlerPosix();
  void Watch(int fd, bool is_real_socket = true) override;
  bool IsChanged(int fd) override;

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

// Wake up handler implementation based on posix sockets. At a high level, this
// class will rely on 2 fake sockets, writing to the write socket on "Set" and
// reading from it on "Clear".
class WakeUpHandlerPosix : public WakeUpHandler {
 public:
  WakeUpHandlerPosix(int fds[2]) : WakeUpHandlerPosix(fds[0], fds[1]) {}
  WakeUpHandlerPosix(int read_fd, int write_fd);
  ~WakeUpHandlerPosix();

  static const WakeUpHandlerPosix* From(const WakeUpHandler* handler) {
    return static_cast<const WakeUpHandlerPosix*>(handler);
  }

  static WakeUpHandlerPosix* From(WakeUpHandler* handler) {
    return static_cast<WakeUpHandlerPosix*>(handler);
  }

  int GetReadHandle() const { return read_fd; }

  // HACK: Open a file descriptor via pipe(2) so that poll/epoll/select can
  // watch it for a signal from the task runner. This way, the file handle can
  // be watched with all other socket file handles, and setting it allows a
  // blocking select call to exit early (as is done in event_waiter.cc).
  // Additionally, this is actually what WebRTC's task runner does (not that
  // that makes it the gold standard):
  // https://webrtc.googlesource.com/src.git/+/refs/heads/master/rtc_base/physical_socket_server.cc#869
  int write_fd;
  int read_fd;

  // Ensure that concurrent sets and clears do not occur in a way that would
  // lead to unexpected behavior.
  std::atomic<bool> is_set;
  std::mutex setter_lock;
};

class EventWaiterPosix : public EventWaiter {
 public:
  explicit EventWaiterPosix(WakeUpHandler* handler);
  ~EventWaiterPosix();
  static const EventWaiterPosix* From(const EventWaiter* waiter) {
    return static_cast<const EventWaiterPosix*>(waiter);
  }

  static EventWaiterPosix* From(EventWaiter* waiter) {
    return static_cast<EventWaiterPosix*>(waiter);
  }

  Error WatchUdpSocketReadable(UdpSocket* socket) override;
  Error StopWatchingUdpSocketReadable(UdpSocket* socket) override;
  Error WatchUdpSocketWritable(UdpSocket* socket) override;
  Error StopWatchingUdpSocketWritable(UdpSocket* socket) override;
  Error WatchNetworkChange() override;
  Error StopWatchingNetworkChange() override;
  ErrorOr<Events> WaitForEvents(Clock::duration timeout) override;
  ErrorOr<Events> WaitForEvents(Clock::duration timeout,
                                SocketHandler* reads,
                                SocketHandler* writes);
  virtual Error WaitForSockets(Clock::duration timeout,
                               SocketHandler* reads,
                               SocketHandler* writes);
  WakeUpHandler* GetWakeUpHandler() override { return this->wake_up_handler; }

 protected:
  std::vector<UdpSocketPosix*> read_sockets;
  std::vector<UdpSocketPosix*> write_sockets;
  WakeUpHandler* wake_up_handler;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_POSIX_EVENT_WAITER_H_
