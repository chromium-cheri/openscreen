// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_UDP_SOCKET_BLOCKING_READ_H_
#define PLATFORM_IMPL_UDP_SOCKET_BLOCKING_READ_H_

#include "platform/api/udp_socket.h"

namespace openscreen {
namespace platform {

struct UdpSocketBlockingRead : public UdpSocket {
 public:
  ~UdpSocketBlockingRead() override = default;

  // Conversion methods, to simplify casting from UdpSocket to
  // UdpSocketBlockingRead.
  static UdpSocketBlockingRead* Convert(UdpSocket* socket) {
    return static_cast<UdpSocketBlockingRead*>(socket);
  }
  static const UdpSocketBlockingRead& Convert(const UdpSocket& socket) {
    return static_cast<const UdpSocketBlockingRead&>(socket);
  }

  // Performs a non-blocking read on the socket, returning the number of bytes
  // received. Note that a non-Error return value of 0 is a valid result,
  // indicating an empty message has been received. Also note that
  // Error::Code::kAgain might be returned if there is no message currently
  // ready for receive, which can be expected during normal operation.
  virtual ErrorOr<UdpPacket> ReceiveMessage() = 0;

 protected:
  UdpSocketBlockingRead() = default;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_UDP_SOCKET_BLOCKING_READ_H_
