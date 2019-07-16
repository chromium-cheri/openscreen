// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef PLATFORM_API_UDP_PACKET_H_
#define PLATFORM_API_UDP_PACKET_H_

#include <vector>

#include "platform/api/logging.h"
#include "platform/base/ip_address.h"

namespace openscreen {
namespace platform {

class UdpSocket;

static constexpr size_t kUdpMaxPacketSize = 1 << 16;

struct UdpPacket : std::vector<uint8_t> {
  UdpPacket() = default;
  explicit UdpPacket(size_t size) : std::vector<uint8_t>(size) {
    OSP_DCHECK(size <= kUdpMaxPacketSize);
  };

  IPEndpoint source = {};
  IPEndpoint original_destination = {};

  // TODO(btolsch): When this gets to implementation, make sure the callback
  // is never called with a |socket| that could have been destroyed (e.g.
  // between queueing the read data and running the task).
  UdpSocket* socket = nullptr;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_UDP_PACKET_H_