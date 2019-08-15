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

#include "absl/types/optional.h"
#include "platform/api/logging.h"
#include "platform/base/error.h"
#include "platform/impl/stream_socket.h"
#include "util/crypto/openssl_util.h"

namespace openscreen {
namespace platform {

// TODO(jophba, rwkeane): implement reading
// TODO(jophba, rwkeane): implement write blocking/unblocking
TlsConnectionPosix::TlsConnectionPosix(IPEndpoint endpoint,
                                       TaskRunner* task_runner)
    : TlsConnection(task_runner),
      socket_(std::make_unique<StreamSocketPosix>(endpoint)) {}

TlsConnectionPosix::~TlsConnectionPosix() = default;

void TlsConnectionPosix::Write(const std::vector<uint8_t>& message) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}

const absl::optional<IPEndpoint>& TlsConnectionPosix::local_address() const {
  return local_address_;
}

const absl::optional<IPEndpoint>& TlsConnectionPosix::remote_address() const {
  return remote_address_;
}

}  // namespace platform
}  // namespace openscreen
