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

namespace {
// constexpr uint8_t kIPAddressIPv6Any[16] {};
}

TlsSocketPosix::TlsSocketPosix(Client* client,
                               const std::string& parent_id,
                               IPEndpoint endpoint)
    : StreamSocketPosix(endpoint),
      client_(client),
      id_(client->GetNewSocketId()),
      parent_id_(parent_id) {}

TlsSocketPosix::~TlsSocketPosix() = default;

bool TlsSocketPosix::IsIPv4() const {
  return address().version() == IPAddress::Version::kV4;
}

bool TlsSocketPosix::IsIPv6() const {
  return address().version() == IPAddress::Version::kV6;
}

void TlsSocketPosix::Close(CloseReason reason) {
  static_cast<StreamSocketPosix*>(this)->Close();
  if (client_) {
    client_->OnClosed(this, reason);
  }
}

void TlsSocketPosix::Write(const TlsSocketMessage& message) {
  // TODO(jophba): how to write?
}

const std::string& TlsSocketPosix::parent_id() const {
  return parent_id_;
}

const std::string& TlsSocketPosix::id() const {
  return id_;
}

}  // namespace platform
}  // namespace openscreen
