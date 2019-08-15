// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/tls_socket_posix.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <memory>

#include "absl/types/optional.h"
#include "platform/api/logging.h"
#include "platform/base/error.h"

namespace openscreen {
namespace platform {

TlsSocketPosix::TlsSocketPosix(Client* client, const std::string& parent_id)
  : TlsSocket(client, parent_id) {}

TlsSocketPosix::~TlsSocketPosix() = default;

bool TlsSocketPosix::IsIPv4() const {
  return stream_socket_.address().has_value() &&
         (stream_socket_.address().value().version() ==
          IPAddress::Version::kV4);
}

bool TlsSocketPosix::IsIPv6() const {
  return stream_socket_.address().has_value() &&
         (stream_socket_.address().value().version() ==
          IPAddress::Version::kV6);
}

void TlsSocketPosix::Close(CloseReason reason) {
  stream_socket_.Close();
  if (client()) {
    client()->OnClosed(this, reason);
  }
}

void TlsSocketPosix::Write(const TlsSocketMessage& message) {

}

}  // namespace platform
}  // namespace openscreen
