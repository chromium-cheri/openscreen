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
#include "platform/api/tcp_socket.h"
#include "platform/base/error.h"
#include "util/crypto/openssl_util.h"

namespace openscreen {
namespace platform {

TlsSocketPosix::TlsSocketPosix(Client* client,
                               const std::string& parent_id,
                               IPEndpoint endpoint)
    : TcpSocketPosix(endpoint),
      client_(client),
      id_(client->GetNewSocketId()),
      parent_id_(parent_id) {}

TlsSocketPosix::~TlsSocketPosix() = default;

bool TlsSocketPosix::IsIPv4() const {
  return GetLocalAddress().version() == IPAddress::Version::kV4;
}

bool TlsSocketPosix::IsIPv6() const {
  return GetLocalAddress().version() == IPAddress::Version::kV6;
}

void TlsSocketPosix::Close(CloseReason reason) {
  static_cast<TcpSocketPosix*>(this)->Close();
  if (client_) {
    client_->OnClosed(this, reason);
  }
}

bool TlsSocketPosix::IsConnected() const {
  return GetState() == TcpSocket::State::kConnected;
}

void TlsSocketPosix::Write(const TlsSocketMessage& message) {
  const int write_result = SSL_write(ssl_.get(), message.data, message.length);

  if (write_result <= 0) {
    client_->OnError(this, GetSSLError(ssl_.get(), write_result));
  }
}

const std::string& TlsSocketPosix::parent_id() const {
  return parent_id_;
}

const std::string& TlsSocketPosix::id() const {
  return id_;
}

// TODO(jophba): implement TlsServerSocketPosix methods
// TODO(jophba): implement OnAccepted, OnMessage client callbacks
}  // namespace platform
}  // namespace openscreen
