// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/tls_connection_factory_posix.h"

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
#include "platform/api/tls_connection_factory.h"
#include "platform/base/error.h"
#include "platform/impl/stream_socket.h"
#include "util/crypto/openssl_util.h"

namespace openscreen {
namespace platform {

ErrorOr<std::unique_ptr<TlsConnectionFactory>> TlsConnectionFactory::Create(
    Client* client) {
  return std::unique_ptr<TlsConnectionFactory>(
      new TlsConnectionFactoryPosix(client));
}

TlsConnectionFactoryPosix::TlsConnectionFactoryPosix(Client* client)
    : TlsConnectionFactory(client) {}

TlsConnectionFactoryPosix::~TlsConnectionFactoryPosix() = default;

void TlsConnectionFactoryPosix::Connect(const IPEndpoint& remote_address,
                                        const TlsConnectOptions& options,
                                        TlsConnection::Client* client) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}

void TlsConnectionFactoryPosix::Listen(const IPEndpoint& local_address,
                                       const TlsCredentials& credentials,
                                       uint32_t backlog_size,
                                       TlsConnection::Client* client) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}
}  // namespace platform
}  // namespace openscreen
