// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_POSIX_UDP_SOCKET_H_
#define PLATFORM_POSIX_UDP_SOCKET_H_

#include "platform/api/udp_socket.h"

namespace openscreen {
namespace platform {

struct UdpSocketPosix : public UdpSocket {
 public:
  UdpSocketPosix(int fd, Version version);
  ~UdpSocketPosix() override;

  // Implementations of UdpSocket methods.
  bool IsIPv4() const override;
  bool IsIPv6() const override;
  Error Bind(const IPEndpoint& local_endpoint) override;
  Error SetMulticastOutboundInterface(NetworkInterfaceIndex ifindex) override;
  Error JoinMulticastGroup(const IPAddress& address,
                           NetworkInterfaceIndex ifindex) override;
  ErrorOr<size_t> ReceiveMessage(void* data,
                                 size_t length,
                                 IPEndpoint* src,
                                 IPEndpoint* original_destination) override;
  Error SendMessage(const void* data,
                    size_t length,
                    const IPEndpoint& dest) override;
  Error SetDscp(DscpMode state) override;

  int GetFd() const { return fd_; }

 private:
  const int fd_;
  const UdpSocket::Version version_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_POSIX_UDP_SOCKET_H_
