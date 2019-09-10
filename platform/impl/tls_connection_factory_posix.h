// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_TLS_CONNECTION_FACTORY_POSIX_H_
#define PLATFORM_IMPL_TLS_CONNECTION_FACTORY_POSIX_H_

#include <memory>
#include <string>

#include "platform/api/tls_connection.h"
#include "platform/api/tls_connection_factory.h"
#include "platform/base/socket_state.h"

namespace openscreen {
namespace platform {

class TlsConnectionFactoryPosix : public TlsConnectionFactory {
 public:
  TlsConnectionFactoryPosix(Client* client, TaskRunner* task_runner);
  ~TlsConnectionFactoryPosix() override;

  // TlsConnectionFactory overrides
  void Connect(const IPEndpoint& remote_address,
               const TlsConnectOptions& options) override;
  void Listen(const IPEndpoint& local_address,
              const TlsCredentials& credentials,
              const TlsListenOptions& options) override;

 private:
  // Ensures that SSL is initialized, then gets a new SSL connection.
  ErrorOr<bssl::UniquePtr<SSL>> GetSslConnection();

  TaskRunner* task_runner_;

  // SSL context, for creating SSL Connections via BoringSSL
  bssl::UniquePtr<SSL_CTX> ssl_context_;
  std::mutex initialization_lock_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_TLS_CONNECTION_FACTORY_POSIX_H_
