// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/event_waiter.h"

#include <sys/select.h>

#include <algorithm>
#include <vector>

#include "platform/api/logging.h"
#include "platform/posix/socket.h"

namespace openscreen {
namespace platform {
namespace {

template <typename T>
WatchResult WatchUdpSocket(std::vector<T>* watched_sockets, T socket) {
  for (const auto* s : *watched_sockets) {
    if (s->fd == socket->fd) {
      return WatchResult::kSetConflict;
    }
  }
  watched_sockets->push_back(socket);
  return WatchResult::kSuccess;
}

template <typename T>
WatchResult StopWatchingUdpSocket(std::vector<T>* watched_sockets, T socket) {
  const auto it = std::find_if(watched_sockets->begin(), watched_sockets->end(),
                               [socket](T s) { return s->fd == socket->fd; });
  if (it == watched_sockets->end()) {
    return WatchResult::kSetConflict;
  }
  watched_sockets->erase(it);
  return WatchResult::kSuccess;
}

}  // namespace

struct EventWaiterPrivate {
  std::vector<UdpSocketIPv4Ptr> v4_read_sockets;
  std::vector<UdpSocketIPv6Ptr> v6_read_sockets;
  std::vector<UdpSocketIPv4Ptr> v4_write_sockets;
  std::vector<UdpSocketIPv6Ptr> v6_write_sockets;
};

EventWaiterPtr CreateEventWaiter() {
  return new EventWaiterPrivate;
}

void DestroyEventWaiter(EventWaiterPtr waiter) {
  delete waiter;
}

WatchResult WatchUdpSocketIPv4Readable(EventWaiterPtr waiter,
                                       UdpSocketIPv4Ptr socket) {
  return WatchUdpSocket(&waiter->v4_read_sockets, socket);
}

WatchResult WatchUdpSocketIPv6Readable(EventWaiterPtr waiter,
                                       UdpSocketIPv6Ptr socket) {
  return WatchUdpSocket(&waiter->v6_read_sockets, socket);
}

WatchResult StopWatchingUdpSocketIPv4Readable(EventWaiterPtr waiter,
                                              UdpSocketIPv4Ptr socket) {
  return StopWatchingUdpSocket(&waiter->v4_read_sockets, socket);
}

WatchResult StopWatchingUdpSocketIPv6Readable(EventWaiterPtr waiter,
                                              UdpSocketIPv6Ptr socket) {
  return StopWatchingUdpSocket(&waiter->v6_read_sockets, socket);
}

WatchResult WatchUdpSocketIPv4Writable(EventWaiterPtr waiter,
                                       UdpSocketIPv4Ptr socket) {
  return WatchUdpSocket(&waiter->v4_write_sockets, socket);
}

WatchResult WatchUdpSocketIPv6Writable(EventWaiterPtr waiter,
                                       UdpSocketIPv6Ptr socket) {
  return WatchUdpSocket(&waiter->v6_write_sockets, socket);
}

WatchResult StopWatchingUdpSocketIPv4Writable(EventWaiterPtr waiter,
                                              UdpSocketIPv4Ptr socket) {
  return StopWatchingUdpSocket(&waiter->v4_write_sockets, socket);
}

WatchResult StopWatchingUdpSocketIPv6Writable(EventWaiterPtr waiter,
                                              UdpSocketIPv6Ptr socket) {
  return StopWatchingUdpSocket(&waiter->v6_write_sockets, socket);
}

WatchResult WatchNetworkChange(EventWaiterPtr waiter) {
  // TODO(btolsch): Implement network change watching.
  UNIMPLEMENTED();
  return WatchResult::kError;
}

WatchResult StopWatchingNetworkChange(EventWaiterPtr waiter) {
  // TODO(btolsch): Implement stop network change watching.
  UNIMPLEMENTED();
  return WatchResult::kError;
}

int WaitForEvents(EventWaiterPtr waiter, TimeDelta timeout, Events* events) {
  int nfds = -1;
  fd_set readfds;
  fd_set writefds;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  for (const auto* read_socket : waiter->v4_read_sockets) {
    FD_SET(read_socket->fd, &readfds);
    nfds = std::max(nfds, read_socket->fd);
  }
  for (const auto* read_socket : waiter->v6_read_sockets) {
    FD_SET(read_socket->fd, &readfds);
    nfds = std::max(nfds, read_socket->fd);
  }
  for (const auto* write_socket : waiter->v4_write_sockets) {
    FD_SET(write_socket->fd, &writefds);
    nfds = std::max(nfds, write_socket->fd);
  }
  for (const auto* write_socket : waiter->v6_write_sockets) {
    FD_SET(write_socket->fd, &writefds);
    nfds = std::max(nfds, write_socket->fd);
  }
  if (nfds == -1) {
    return 0;
  }

  struct timeval tv;
  tv.tv_sec = timeout.AsSeconds();
  tv.tv_usec = timeout.AsMicroseconds() % 1000000;
  const int rv = select(nfds, &readfds, &writefds, nullptr, &tv);
  if (rv == -1 || rv == 0) {
    return rv;
  }
  for (auto* read_socket : waiter->v4_read_sockets) {
    if (FD_ISSET(read_socket->fd, &readfds)) {
      events->udpv4_readable_events.push_back({read_socket});
    }
  }
  for (auto* read_socket : waiter->v6_read_sockets) {
    if (FD_ISSET(read_socket->fd, &readfds)) {
      events->udpv6_readable_events.push_back({read_socket});
    }
  }
  for (auto* write_socket : waiter->v4_write_sockets) {
    if (FD_ISSET(write_socket->fd, &writefds)) {
      events->udpv4_writable_events.push_back({write_socket});
    }
  }
  for (auto* write_socket : waiter->v6_write_sockets) {
    if (FD_ISSET(write_socket->fd, &writefds)) {
      events->udpv6_writable_events.push_back({write_socket});
    }
  }
  return 1;
}

}  // namespace platform
}  // namespace openscreen
