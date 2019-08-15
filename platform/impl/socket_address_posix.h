// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_SOCKET_ADDRESS_POSIX_H_
#define PLATFORM_IMPL_SOCKET_ADDRESS_POSIX_H_

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#include "platform/base/ip_address.h"

namespace openscreen {
namespace platform {

// The way the sockaddr_* family works in POSIX is pretty unintuitive. The
// sockaddr_in and sockaddr_in6 structs can be reinterpreted as type
// sockaddr, however they don't have a common parent--the types are unrelated.
// Our solution for this is to wrap sockaddr_in* in a union, so that our code
// can be simplified since most platform APIs just take a sockaddr.
union SocketAddressIn {
  struct sockaddr_in v4;
  struct sockaddr_in6 v6;
};

class SocketAddressPosix {
 public:
  SocketAddressPosix(const IPEndpoint& endpoint);

  SocketAddressPosix(const SocketAddressPosix&) = default;
  SocketAddressPosix(SocketAddressPosix&&) = default;
  SocketAddressPosix& operator=(const SocketAddressPosix&) = default;
  SocketAddressPosix& operator=(SocketAddressPosix&&) = default;

  struct sockaddr* address() {
    switch (version_) {
      case (IPAddress::Version::kV4):
        return reinterpret_cast<struct sockaddr*>(&internal_address_.v4);
      case (IPAddress::Version::kV6):
        return reinterpret_cast<struct sockaddr*>(&internal_address_.v6);
    }
  }

  const struct sockaddr* address() const {
    switch (version_) {
      case (IPAddress::Version::kV4):
        return reinterpret_cast<const struct sockaddr*>(&internal_address_.v4);
      case (IPAddress::Version::kV6):
        return reinterpret_cast<const struct sockaddr*>(&internal_address_.v6);
    }
  }

  IPAddress::Version version() const { return version_; }

  // Some platform APIs change the size of the sockaddr.
  void set_size(socklen_t size) { size_ = size; };
  socklen_t size() const { return size_; }

 private:
  SocketAddressIn internal_address_;

  // Currently we only support IPv4
  IPAddress::Version version_;
  socklen_t size_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_SOCKET_ADDRESS_POSIX_H_
