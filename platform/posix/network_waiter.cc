// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/posix/network_waiter.h"

#include <time.h>

#include <algorithm>
#include <vector>

#include "osp_base/error.h"
#include "platform/api/logging.h"
#include "platform/base/network_reader.h"
#include "platform/posix/udp_socket.h"

namespace openscreen {
namespace platform {

NetworkWaiterPosix::NetworkWaiterPosix() = default;

NetworkWaiterPosix::~NetworkWaiterPosix() = default;

ErrorOr<std::vector<UdpSocket*>> NetworkWaiterPosix::AwaitSocketsReadable(
    const std::vector<UdpSocket*>& sockets,
    const Clock::duration& timeout) {
  int max_fd = -1;
  FD_ZERO(&read_handles_);
  for (UdpSocket* socket : sockets) {
    UdpSocketPosix* posix_socket = UdpSocketPosix::From(socket);
    FD_SET(posix_socket->fd, &read_handles_);
    max_fd = std::max(max_fd, posix_socket->fd);
  }
  if (max_fd < 0) {
    return Error::Code::kIOFailure;
  }

  struct timeval tv = ToTimeval(timeout);
  // This value is set to 'max_fd + 1' by convention. For more information, see:
  // http://man7.org/linux/man-pages/man2/select.2.html
  int max_fd_to_watch = max_fd + 1;
  const int rv = select(max_fd_to_watch, &read_handles_, nullptr, nullptr, &tv);
  if (rv == -1) {
    // This is the case when an error condition is hit within the select(...)
    // command.
    return Error::Code::kIOFailure;
  } else if (rv == 0) {
    // This occurs when no sockets have a pending read.
    return Error::Code::kAgain;
  }

  std::vector<UdpSocket*> changed_sockets;
  for (UdpSocket* socket : sockets) {
    UdpSocketPosix* posix_socket = UdpSocketPosix::From(socket);
    if (FD_ISSET(posix_socket->fd, &read_handles_)) {
      changed_sockets.push_back(socket);
    }
  }

  return changed_sockets;
}

// static
std::unique_ptr<NetworkWaiter> NetworkWaiter::Create() {
  return std::unique_ptr<NetworkWaiter>(new NetworkWaiterPosix());
}

// static
struct timeval NetworkWaiterPosix::ToTimeval(const Clock::duration& timeout) {
  struct timeval tv;
  constexpr uint64_t max_microseconds = uint64_t{1000000};
  uint64_t microseconds =
      static_cast<uint64_t>(std::chrono::microseconds(timeout).count());
  tv.tv_sec = microseconds / (max_microseconds);
  tv.tv_usec = microseconds % (max_microseconds);

  return tv;
}

}  // namespace platform
}  // namespace openscreen
