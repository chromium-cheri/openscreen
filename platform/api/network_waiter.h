// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_NETWORK_WAITER_H_
#define PLATFORM_API_NETWORK_WAITER_H_

#include <vector>

#include "osp_base/error.h"
#include "platform/api/time.h"
#include "platform/api/udp_socket.h"

namespace openscreen {
namespace platform {

// The class responsible for calling platform-level method to watch UDP sockets
// for available read data. Reading from these sockets is handled at a higher
// layer.
class NetworkWaiter {
 public:
  // Creates a new EventWaiter instance.
  static std::unique_ptr<NetworkWaiter> Create();

  // Virtual destructor needed to support delete call from unique_ptr.
  virtual ~NetworkWaiter() = default;

  // Waits until the sockets provided to these SocketHandlers determine a read
  // has occured, or the provided timeout has passed.
  virtual ErrorOr<std::vector<UdpSocket*>> AwaitSocketsReadable(
      const std::vector<UdpSocket*>& sockets,
      const Clock::duration& timeout) = 0;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_NETWORK_WAITER_H_
