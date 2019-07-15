// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef PLATFORM_API_UDP_READ_CALLBACK_H_
#define PLATFORM_API_UDP_READ_CALLBACK_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "platform/base/ip_address.h"

namespace openscreen {
namespace platform {

class NetworkRunner;
class UdpSocket;

static constexpr int kUdpMaxPacketSize = 1 << 16;

class UdpReadCallback {
 public:
  struct Packet : std::vector<uint8_t> {
    Packet() = default;
    explicit Packet(size_t size) : std::vector<uint8_t>(size){};

    IPEndpoint source;
    IPEndpoint original_destination;

    // TODO(btolsch): When this gets to implementation, make sure the callback
    // is never called with a |socket| that could have been destroyed (e.g.
    // between queueing the read data and running the task).
    UdpSocket* socket;
  };

  virtual ~UdpReadCallback() = default;

  virtual void OnRead(Packet data, NetworkRunner* network_runner) = 0;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_UDP_READ_CALLBACK_H_
