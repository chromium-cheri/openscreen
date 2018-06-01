// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/event_waiter.h"

#include <sys/select.h>

#include <algorithm>
#include <vector>

#include "platform/api/logging.h"
#include "platform/linux/socket.h"

namespace openscreen {
namespace platform {

struct EventWaiterPrivate {
  std::vector<UdpSocketIPv4Ptr> v4_read_sockets;
  std::vector<UdpSocketIPv6Ptr> v6_read_sockets;
};

EventWaiterPtr CreateEventWaiter() {
  return new EventWaiterPrivate;
}

void DestroyEventWaiter(EventWaiterPtr waiter) {
  delete waiter;
}

bool WatchUdpSocketIPv4Readable(EventWaiterPtr waiter,
                                UdpSocketIPv4Ptr socket) {
  DCHECK(waiter);
  DCHECK(socket);
  if (std::find_if(begin(waiter->v4_read_sockets), end(waiter->v4_read_sockets),
                   [socket](UdpSocketIPv4Ptr s) {
                     return s->fd == socket->fd;
                   }) != end(waiter->v4_read_sockets)) {
    return false;
  }
  waiter->v4_read_sockets.push_back(socket);
  return true;
}

bool WatchUdpSocketIPv6Readable(EventWaiterPtr waiter,
                                UdpSocketIPv6Ptr socket) {
  DCHECK(waiter);
  DCHECK(socket);
  if (std::find_if(begin(waiter->v6_read_sockets), end(waiter->v6_read_sockets),
                   [socket](UdpSocketIPv6Ptr s) {
                     return s->fd == socket->fd;
                   }) != end(waiter->v6_read_sockets)) {
    return false;
  }
  waiter->v6_read_sockets.push_back(socket);
  return true;
}

bool StopWatchingUdpSocketIPv4Readable(EventWaiterPtr waiter,
                                       UdpSocketIPv4Ptr socket) {
  DCHECK(waiter);
  DCHECK(socket);
  LOG_ERROR << __func__ << ": unimplemented";
  return false;
}

bool StopWatchingUdpSocketIPv6Readable(EventWaiterPtr waiter,
                                       UdpSocketIPv6Ptr socket) {
  DCHECK(waiter);
  DCHECK(socket);
  LOG_ERROR << __func__ << ": unimplemented";
  return false;
}

bool WatchNetworkChange(EventWaiterPtr waiter) {
  DCHECK(waiter);
  LOG_ERROR << __func__ << ": unimplemented";
  return false;
}

bool StopWatchingNetworkChange(EventWaiterPtr waiter) {
  DCHECK(waiter);
  LOG_ERROR << __func__ << ": unimplemented";
  return false;
}

bool WaitForEvents(const std::vector<EventWaiterPtr>& waiters,
                   Milliseconds timeout,
                   EventMap* event_map) {
  DCHECK(event_map);
  int nfds = -1;
  fd_set readfds;
  FD_ZERO(&readfds);
  for (const auto* waiter : waiters) {
    DCHECK(waiter);
    for (const auto* read_socket : waiter->v4_read_sockets) {
      FD_SET(read_socket->fd, &readfds);
      if (read_socket->fd > nfds) {
        nfds = read_socket->fd;
      }
    }
    for (const auto* read_socket : waiter->v6_read_sockets) {
      FD_SET(read_socket->fd, &readfds);
      if (read_socket->fd > nfds) {
        nfds = read_socket->fd;
      }
    }
  }
  if (nfds == -1) {
    return false;
  }

  struct timeval tv;
  tv.tv_sec = timeout.t / 1000;
  tv.tv_usec = (timeout.t % 1000) * 1000;
  const int rv = select(nfds, &readfds, nullptr, nullptr, &tv);
  if (rv == -1 || rv == 0) {
    return false;
  }
  for (auto* waiter : waiters) {
    Events events;
    for (auto* read_socket : waiter->v4_read_sockets) {
      if (FD_ISSET(read_socket->fd, &readfds)) {
        events.udpv4_readable_events.push_back({read_socket});
      }
    }
    for (auto* read_socket : waiter->v6_read_sockets) {
      if (FD_ISSET(read_socket->fd, &readfds)) {
        events.udpv6_readable_events.push_back({read_socket});
      }
    }
    event_map->emplace(waiter, std::move(events));
  }
  return true;
}

}  // namespace platform
}  // namespace openscreen
