// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/socket_address_posix.h"

namespace openscreen {
namespace platform {

SocketAddressPosix::SocketAddressPosix(const IPEndpoint& endpoint) {
  if (endpoint.address.IsV4()) {
    struct sockaddr_in socket_address_v4;
    socket_address_v4.sin_family = AF_INET;
    socket_address_v4.sin_port = htons(endpoint.port);
    endpoint.address.CopyToV4(
        reinterpret_cast<uint8_t*>(&socket_address_v4.sin_addr.s_addr));
    is_v4_ = true;
    internal_address_.v4 = socket_address_v4;
    size_ = sizeof(socket_address_v4);
  } else {
    struct sockaddr_in6 socket_address_v6;
    socket_address_v6.sin6_family = AF_INET6;
    socket_address_v6.sin6_flowinfo = 0;
    socket_address_v6.sin6_scope_id = 0;
    socket_address_v6.sin6_port = htons(endpoint.port);
    endpoint.address.CopyToV6(
        reinterpret_cast<uint8_t*>(&socket_address_v6.sin6_addr));
    is_v4_ = false;
    internal_address_.v6 = socket_address_v6;
    size_ = sizeof(socket_address_v6);
  }
}

}  // namespace platform
}  // namespace openscreen
