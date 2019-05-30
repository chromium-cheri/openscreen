// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/posix/posix_socket.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <memory>

#include "platform/api/logging.h"

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

// Examine |posix_errno| to determine whether the specific cause of a failure
// was transient or hard, and return the appropriate error response.
Error ChooseError(decltype(errno) posix_errno, Error::Code hard_error_code) {
  if (posix_errno == EAGAIN || posix_errno == EWOULDBLOCK ||
      posix_errno == ENOBUFS) {
    return Error(Error::Code::kAgain, strerror(errno));
  }
  return Error(hard_error_code, strerror(errno));
}

}  // namespace

ErrorOr<PosixSocket> PosixSocket::Create(SocketType type,
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
  return PosixSocket(fd, version);
}

bool PosixSocket::operator=(const PosixSocket& other) {
  return (file_descriptor_ == other.file_descriptor_)
  && (version_ == other.version_);
}

Error PosixSocket::SendMessage(const void* data,
                               size_t length,
                               const IPEndpoint& dest) {
  struct iovec iov = {const_cast<void*>(data), length};
  struct msghdr msg;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = nullptr;
  msg.msg_controllen = 0;
  msg.msg_flags = 0;

  switch (version_) {
    case Version::kV4: {
      struct sockaddr_in sa = {
          .sin_family = AF_INET,
          .sin_port = htons(dest.port),
      };
      dest.address.CopyToV4(reinterpret_cast<uint8_t*>(&sa.sin_addr.s_addr));
      msg.msg_name = &sa;
      msg.msg_namelen = sizeof(sa);
      break;
    }

    case Version::kV6: {
      struct sockaddr_in6 sa = {
          .sin6_family = AF_INET6,
          .sin6_flowinfo = 0,
          .sin6_scope_id = 0,
          .sin6_port = htons(dest.port),
      };
      dest.address.CopyToV6(reinterpret_cast<uint8_t*>(&sa.sin6_addr.s6_addr));
      msg.msg_name = &sa;
      msg.msg_namelen = sizeof(sa);
      break;
    }

    default:
      OSP_NOTREACHED();
      break;
  }

  const ssize_t num_bytes_sent = sendmsg(file_descriptor_, &msg, 0);
  if (num_bytes_sent == -1) {
    return ChooseError(errno, Error::Code::kSocketSendFailure);
  }

  // Sanity-check: UDP datagram sendmsg() is all or nothing.
  OSP_DCHECK_EQ(static_cast<size_t>(num_bytes_sent), length);
  return Error::Code::kNone;
}
}  // namespace platform
}  // namespace openscreen
