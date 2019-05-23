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
#include "platform/posix/socket.h"

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

SocketHandlerPosix::SocketHandlerPosix() {
  FD_ZERO(&set);
  max_fd = -1;
}

void SocketHandlerPosix::Watch(int fd, bool is_real_socket) {
  FD_SET(fd, &set);
  if (is_real_socket) {
    max_fd = std::max(max_fd, fd);
  }
}

bool SocketHandlerPosix::IsChanged(int fd) {
  return FD_ISSET(fd, &set);
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

EventWaiterPosix::EventWaiterPosix(WakeUpHandler* handler)
    : wake_up_handler(handler) {}

EventWaiterPosix::~EventWaiterPosix() {
  delete wake_up_handler;
}

Error EventWaiterPosix::WatchUdpSocketReadable(UdpSocket* socket) {
  return AddToVectorIfMissing(UdpSocketPosix::From(socket),
                              &this->read_sockets);
}

Error EventWaiterPosix::StopWatchingUdpSocketReadable(UdpSocket* socket) {
  return RemoveFromVectorIfPresent(UdpSocketPosix::From(socket),
                                   &this->read_sockets);
}

Error EventWaiterPosix::WatchUdpSocketWritable(UdpSocket* socket) {
  return AddToVectorIfMissing(UdpSocketPosix::From(socket),
                              &this->write_sockets);
}

Error EventWaiterPosix::StopWatchingUdpSocketWritable(UdpSocket* socket) {
  return RemoveFromVectorIfPresent(UdpSocketPosix::From(socket),
                                   &this->write_sockets);
}

Error EventWaiterPosix::WatchNetworkChange() {
  // TODO(btolsch): Implement network change watching.
  OSP_UNIMPLEMENTED();
  return Error::Code::kNotImplemented;
}

Error EventWaiterPosix::StopWatchingNetworkChange() {
  // TODO(btolsch): Implement stop network change watching.
  OSP_UNIMPLEMENTED();
  return Error::Code::kNotImplemented;
}

ErrorOr<Events> EventWaiterPosix::WaitForEvents(Clock::duration timeout) {
  SocketHandlerPosix read_handler;
  SocketHandlerPosix write_handler;
  return WaitForEvents(timeout, &read_handler, &write_handler);
}

ErrorOr<Events> EventWaiterPosix::WaitForEvents(Clock::duration timeout,
                                                SocketHandler* reads,
                                                SocketHandler* writes) {
  WakeUpHandlerPosix* handler = WakeUpHandlerPosix::From(this->wake_up_handler);

  reads->Watch(handler->GetReadHandle(), false);
  for (const auto* read_socket : this->read_sockets) {
    reads->Watch(read_socket->fd);
  }
  for (const auto* write_socket : this->write_sockets) {
    writes->Watch(write_socket->fd);
  }

  auto error = WaitForSockets(timeout, reads, writes);
  if (error.code() != Error::Code::kNone) {
    return error;
  }

  Events events;
  if (reads->IsChanged(handler->GetReadHandle())) {
    handler->Clear();
  }
  for (auto* read_socket : this->read_sockets) {
    if (reads->IsChanged(read_socket->fd)) {
      events.udp_readable_events.push_back({read_socket});
    }
  }
  for (auto* write_socket : this->write_sockets) {
    if (writes->IsChanged(write_socket->fd)) {
      events.udp_writable_events.push_back({write_socket});
    }
  }

  return std::move(events);
}

Error EventWaiterPosix::WaitForSockets(Clock::duration timeout,
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
EventWaiter* EventWaiter::Create() {
  return new EventWaiterPosix(WakeUpHandler::Create());
}

WakeUpHandlerPosix::WakeUpHandlerPosix(int read, int write)
    : write_fd(write), read_fd(read) {
  is_set.store(false);
}

WakeUpHandlerPosix::~WakeUpHandlerPosix() = default;

void WakeUpHandler::Set() {
  WakeUpHandlerPosix* handler = WakeUpHandlerPosix::From(this);
  if (handler->is_set.load()) {
    return;
  }

  std::lock_guard<std::mutex> lock(handler->setter_lock);
  if (handler->is_set.load()) {
    return;
  }
  uint8_t byte = uint8_t{0x1};
  write(handler->write_fd, &byte, sizeof(byte));
  handler->is_set.store(true);
}

void WakeUpHandler::Clear() {
  WakeUpHandlerPosix* handler = WakeUpHandlerPosix::From(this);
  if (!handler->is_set.load()) {
    return;
  }

  std::lock_guard<std::mutex> lock(handler->setter_lock);
  if (!handler->is_set.load()) {
    return;
  }
  uint8_t result = 0;
  read(handler->read_fd, &result, sizeof(result));
  handler->is_set.store(false);
}

// static
WakeUpHandler* WakeUpHandler::Create() {
  int pipefds[2];
  if (pipe(pipefds) == -1) {
    return nullptr;
  }

  return new WakeUpHandlerPosix(pipefds);
}

}  // namespace platform
}  // namespace openscreen
