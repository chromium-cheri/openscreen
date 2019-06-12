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
namespace {}  // namespace

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
