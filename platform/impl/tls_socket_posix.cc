// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/tls_socket_posix.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <openssl/ssl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <memory>

#include "absl/types/optional.h"
#include "platform/api/logging.h"
#include "platform/base/error.h"
#include "platform/impl/stream_socket.h"
#include "util/crypto/openssl_util.h"

namespace openscreen {
namespace platform {

TlsSocketPosix::TlsSocketPosix(Client* client,
                               const std::string& parent_id,
                               IPEndpoint endpoint)
    : TlsSocket(client),
      id_(client->GetNewSocketId()),
      parent_id_(parent_id),
      socket_(std::make_unique<StreamSocketPosix>(endpoint)) {}

TlsSocketPosix::~TlsSocketPosix() = default;

bool TlsSocketPosix::IsIPv4() const {
  return socket_->version() == IPAddress::Version::kV4;
}

bool TlsSocketPosix::IsIPv6() const {
  return socket_->version() == IPAddress::Version::kV6;
}

void TlsSocketPosix::Close(CloseReason reason) {
  socket_->Close();
  if (client()) {
    client()->OnClosed(this, reason);
  }
}

SocketState TlsSocketPosix::state() const {
  return socket_->state();
}

void TlsSocketPosix::Write(const TlsPacket& message) {
  const int write_result = SSL_write(ssl_.get(), message.data, message.length);

  if (write_result <= 0) {
    client()->OnError(this, GetSSLError(ssl_.get(), write_result));
  }
}

absl::optional<std::string> TlsSocketPosix::parent_server_socket_id() const {
  return parent_id_;
}

const std::string& TlsSocketPosix::id() const {
  return id_;
}

// TODO(jophba): implement OnAccepted, OnMessage client callbacks
}  // namespace platform
}  // namespace openscreen
