// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_TLS_CONNECTION_FACTORY_POSIX_H_
#define PLATFORM_IMPL_TLS_CONNECTION_FACTORY_POSIX_H_

#include <memory>
#include <string>

#include "platform/api/socket_state.h"
#include "platform/api/tls_connection.h"
#include "platform/api/tls_connection_factory.h"

namespace openscreen {
namespace platform {

class TlsConnectionFactoryPosix : public TlsConnectionFactory {
 public:
  TlsConnectionFactoryPosix(Client* client,
                            const std::string& parent_id,
                            IPEndpoint local_endpoint);
  ~TlsConnectionFactoryPosix();

  // TlsConnectionFactory overrides
  void Connect(const IPEndpoint& remote_address,
               const TlsConnectOptions& options,
               TlsConnection::Client* client) override;
  void Listen(const IPEndpoint& local_address,
              const TlsCredentials& credentials,
              uint32_t backlog_size,
              TlsConnection::Client* client) override;

 protected:
  friend class TlsConnectionFactory;
  TlsConnectionFactoryPosix(Client* client);

 private:
  const absl::optional<IPEndpoint> local_address_;
  const absl::optional<IPEndpoint> remote_address_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_TLS_CONNECTION_FACTORY_POSIX_H_
