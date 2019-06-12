// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/posix/event_waiter.h"

#include <sys/select.h>
#include <time.h>

#include <algorithm>
#include <vector>

#include "osp_base/error.h"
#include "platform/api/logging.h"
#include "platform/base/network_loop.h"
#include "platform/posix/udp_socket.h"

namespace openscreen {
namespace platform {
namespace {

Error AddToVectorIfMissing(UdpSocketPosix* socket,
                           std::vector<UdpSocketPosix*>* watched_sockets) {
  for (const auto* s : *watched_sockets) {
    if (s->fd == socket->fd)
      return Error::Code::kAlreadyListening;
  }
  watched_sockets->push_back(socket);
  return Error::None();
}

Error RemoveFromVectorIfPresent(UdpSocketPosix* socket,
                                std::vector<UdpSocketPosix*>* watched_sockets) {
  const auto it =
      std::find_if(watched_sockets->begin(), watched_sockets->end(),
                   [socket](UdpSocketPosix* s) { return s->fd == socket->fd; });
  if (it == watched_sockets->end())
    return Error::Code::kNoItemFound;

  watched_sockets->erase(it);
  return Error::None();
}

}  // namespace

struct EventWaiterPrivate {
  std::vector<UdpSocketPosix*> read_sockets;
  std::vector<UdpSocketPosix*> write_sockets;
};

EventWaiterPtr CreateEventWaiter() {
  return new EventWaiterPrivate;
}

void DestroyEventWaiter(EventWaiterPtr waiter) {
  delete waiter;
}

Error WatchUdpSocketReadable(EventWaiterPtr waiter, UdpSocket* socket) {
  return AddToVectorIfMissing(UdpSocketPosix::From(socket),
                              &waiter->read_sockets);
}

Error StopWatchingUdpSocketReadable(EventWaiterPtr waiter, UdpSocket* socket) {
  return RemoveFromVectorIfPresent(UdpSocketPosix::From(socket),
                                   &waiter->read_sockets);
}

Error WatchUdpSocketWritable(EventWaiterPtr waiter, UdpSocket* socket) {
  return AddToVectorIfMissing(UdpSocketPosix::From(socket),
                              &waiter->write_sockets);
}

Error StopWatchingUdpSocketWritable(EventWaiterPtr waiter, UdpSocket* socket) {
  return RemoveFromVectorIfPresent(UdpSocketPosix::From(socket),
                                   &waiter->write_sockets);
}

Error WatchNetworkChange(EventWaiterPtr waiter) {
  // TODO(btolsch): Implement network change watching.
  OSP_UNIMPLEMENTED();
  return Error::Code::kNotImplemented;
}

Error StopWatchingNetworkChange(EventWaiterPtr waiter) {
  // TODO(btolsch): Implement stop network change watching.
  OSP_UNIMPLEMENTED();
  return Error::Code::kNotImplemented;
}

ErrorOr<Events> WaitForEvents(EventWaiterPtr waiter) {
  int max_fd = -1;
  fd_set readfds;
  fd_set writefds;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  for (const auto* read_socket : waiter->read_sockets) {
    FD_SET(read_socket->fd, &readfds);
    max_fd = std::max(max_fd, read_socket->fd);
  }
  for (const auto* write_socket : waiter->write_sockets) {
    FD_SET(write_socket->fd, &writefds);
    max_fd = std::max(max_fd, write_socket->fd);
  }
  if (max_fd == -1)
    return Error::Code::kIOFailure;

  struct timeval tv = {};
  const int rv = select(max_fd + 1, &readfds, &writefds, nullptr, &tv);
  if (rv == -1 || rv == 0)
    return Error::Code::kIOFailure;

  Events events;
  for (auto* read_socket : waiter->read_sockets) {
    if (FD_ISSET(read_socket->fd, &readfds))
      events.udp_readable_events.push_back({read_socket});
  }
  for (auto* write_socket : waiter->write_sockets) {
    if (FD_ISSET(write_socket->fd, &writefds))
      events.udp_writable_events.push_back({write_socket});
  }

  return std::move(events);
}


SocketHandlerPosix::~SocketHandlerPosix() = default;

SocketHandlerPosix::SocketHandlerPosix() {
  Clear();
  max_fd = -1;
}

void SocketHandlerPosix::Watch(const UdpSocket& socket) {
  const auto& posix_socket = UdpSocketPosix::From(socket);
  FD_SET(posix_socket.fd, &set);
  max_fd = std::max(max_fd, posix_socket.fd);
}

bool SocketHandlerPosix::IsChanged(const UdpSocket& socket) {
  const auto& posix_socket = UdpSocketPosix::From(socket);
  return FD_ISSET(posix_socket.fd, &set);
}

void SocketHandlerPosix::Clear() {
  FD_ZERO(&set);
}

// static
Error SocketHandlerPosix::WatchForChanges(SocketHandlerPosix* read_handles,
                                          SocketHandlerPosix* write_handles,
                                          Clock::duration timeout) {
  struct timeval tv = ToTimeval(timeout);

  int max_fd = std::max(read_handles->max_fd, write_handles->max_fd);
  const int rv =
      select(max_fd + 1, &read_handles->set, &write_handles->set, nullptr, &tv);
  if (rv == -1 || rv == 0) {
    return Error::Code::kIOFailure;
  }

  return Error::None();
}

// static
struct timeval SocketHandlerPosix::ToTimeval(Clock::duration timeout) {
  struct timeval tv;
  constexpr uint64_t max_microseconds = uint64_t{1000000};
  uint64_t microseconds =
      static_cast<uint64_t>(std::chrono::microseconds(timeout).count());
  tv.tv_sec = microseconds / (max_microseconds);
  tv.tv_usec = microseconds % (max_microseconds);

  return tv;
}

// static
std::unique_ptr<SocketHandler> SocketHandler::Create() {
  return std::unique_ptr<SocketHandler>(new SocketHandlerPosix());
}

EventWaiterPosix::EventWaiterPosix() = default;

EventWaiterPosix::~EventWaiterPosix() = default;

Error EventWaiterPosix::WaitForEvents(Clock::duration timeout,
                                      SocketHandler* reads,
                                      SocketHandler* writes) {
  auto* read_handler = static_cast<SocketHandlerPosix*>(reads);
  auto* write_handler = static_cast<SocketHandlerPosix*>(writes);
  if (read_handler->max_fd < 0 && write_handler->max_fd < 0) {
    return Error::Code::kIOFailure;
  }

  return SocketHandlerPosix::WatchForChanges(read_handler, write_handler,
                                             timeout);
}

// static
std::unique_ptr<EventWaiter> EventWaiter::Create() {
  return std::unique_ptr<EventWaiter>(new EventWaiterPosix());
}

}  // namespace platform
}  // namespace openscreen
