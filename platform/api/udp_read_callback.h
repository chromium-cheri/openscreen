// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef PLATFORM_API_UDP_READ_CALLBACK_H_
#define PLATFORM_API_UDP_READ_CALLBACK_H_

#include "platform/api/udp_packet.h"

namespace openscreen {
namespace platform {

class NetworkRunner;

class UdpReadCallback {
 public:
  virtual ~UdpReadCallback() = default;
  virtual void OnRead(UdpPacket data, NetworkRunner* network_runner) = 0;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_UDP_READ_CALLBACK_H_
