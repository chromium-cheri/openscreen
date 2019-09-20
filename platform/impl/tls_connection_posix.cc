// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/tls_connection_posix.h"

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
#include <utility>

#include "absl/types/optional.h"
#include "platform/api/logging.h"
#include "platform/base/error.h"
#include "platform/impl/stream_socket.h"
#include "util/crypto/openssl_util.h"

namespace openscreen {
namespace platform {

// TODO(jophba, rwkeane): implement write blocking/unblocking
TlsConnectionPosix::TlsConnectionPosix(IPEndpoint local_address,
                                       TaskRunner* task_runner)
    : TlsConnection(task_runner), socket_(local_address), buffer_(this) {}

TlsConnectionPosix::TlsConnectionPosix(IPAddress::Version version,
                                       TaskRunner* task_runner)
    : TlsConnection(task_runner), socket_(version), buffer_(this) {}

TlsConnectionPosix::~TlsConnectionPosix() = default;

void TlsConnectionPosix::ReceiveMessage() {
  const int bytes_available = SSL_pending(ssl_.get());
  if (bytes_available > 0) {
    // NOTE: the pending size of the data block available is not a guarantee
    // that will receive only bytes_available or even
    // any data, since not all pending bytes are application data.
    std::vector<uint8_t> block(bytes_available);

    const int bytes_read = SSL_read(ssl_.get(), block.data(), bytes_available);

    // Read operator was not successful, either due to a closed connection,
    // an error occurred, or we have to take an action.
    if (bytes_read < 0) {
      // TODO(jophba): improve error reporting to client. If we call OnError
      // with kAgain, how do they know what to do again?
      client()->OnError(this, GetSSLError(ssl_.get(), bytes_read));
      return;
    }

    if (bytes_read < bytes_available) {
      block.resize(bytes_read);
    }

    client()->OnRead(this, std::move(block));
  }
}

void TlsConnectionPosix::Write(const void* data, size_t len) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}

const IPEndpoint& TlsConnectionPosix::local_address() const {
  const absl::optional<IPEndpoint> endpoint = socket_.local_address();
  OSP_DCHECK(endpoint.has_value());
  return endpoint.value();
}

const IPEndpoint& TlsConnectionPosix::remote_address() const {
  const absl::optional<IPEndpoint> endpoint = socket_.remote_address();
  OSP_DCHECK(endpoint.has_value());
  return endpoint.value();
}

void TlsConnectionPosix::NotifyWriteBufferFill(double fraction) {
  constexpr double kBlockBufferPercentage = 0.5;
  if (fraction > kBlockBufferPercentage && !is_buffer_blocked_) {
    OnWriteBlocked();
    is_buffer_blocked_ = true;
  } else if (fraction < kBlockBufferPercentage && is_buffer_blocked_) {
    OnWriteUnblocked();
    is_buffer_blocked_ = false;
  } else if (fraction >= 0.99 && is_buffer_blocked_) {
    OnError(Error::Code::kInsufficientBuffer);
  }
}

}  // namespace platform
}  // namespace openscreen
