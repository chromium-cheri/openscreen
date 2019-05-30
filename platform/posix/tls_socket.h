// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_POSIX_TLS_SOCKET_H_
#define PLATFORM_POSIX_TLS_SOCKET_H_

#include "platform/api/tls_socket.h"

namespace openscreen {
namespace platform {

struct TlsSocketPosix : public TlsSocket {
  TlsSocketPosix(int fd, IPAddress::Version version);

  static const TlsSocketPosix* From(const TlsSocket* socket) {
    return static_cast<const TlsSocketPosix*>(socket);
  }

  static TlsSocketPosix* From(TlsSocket* socket) {
    return static_cast<TlsSocketPosix*>(socket);
  }

  const int fd;
  const IPAddress::Version version;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_POSIX_TLS_SOCKET_H_
