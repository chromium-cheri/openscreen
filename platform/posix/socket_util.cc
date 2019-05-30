// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/posix/socket_util.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <memory>

namespace openscreen {
namespace platform {
namespace {
int ConvertToRawSocketType(SocketType type) {
  switch (type) {
    case SocketType::Tcp:
      return SOCK_STREAM;
    case SocketType::Udp:
      return SOCK_DGRAM;
  }
}

int ConvertToDomain(IPAddress::Version version) {
  switch (version) {
    case IPAddress::Version::kV4:
      return AF_INET;
    case IPAddress::Version::kV6:
      return AF_INET6;
  }
}
}  // namespace

ErrorOr<int> CreateNonBlockingSocket(SocketType type,
                                     IPAddress::Version version) {
  int fd = socket(ConvertToDomain(version), ConvertToRawSocketType(type), 0);
  if (fd == -1) {
    return Error(Error::Code::kInitializationFailure, strerror(errno));
  }
  // On non-Linux, the SOCK_NONBLOCK option is not available, so use the
  // more-portable method of calling fcntl() to set this behavior.
  if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) == -1) {
    close(fd);
    return Error(Error::Code::kInitializationFailure, strerror(errno));
  }
  return fd;
}
}  // namespace platform
}  // namespace openscreen