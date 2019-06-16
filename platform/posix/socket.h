// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_POSIX_SOCKET_H_
#define PLATFORM_POSIX_SOCKET_H_

#include "platform/api/socket.h"

namespace openscreen {
namespace platform {

struct SocketPosix : public Socket {
  const int fd;
  const Socket::Version version;

  SocketPosix(int fd, Version version, std::string id, Delegate* delegate);

  static const SocketPosix* From(const Socket* socket) {
    return static_cast<const SocketPosix*>(socket);
  }

  static SocketPosix* From(Socket* socket) {
    return static_cast<SocketPosix*>(socket);
  }
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_POSIX_SOCKET_H_
