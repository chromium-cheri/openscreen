// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_TLS_CONNECTION_POSIX_H_
#define PLATFORM_IMPL_TLS_CONNECTION_POSIX_H_

#include <memory>
#include <string>
#include <vector>

#include "platform/api/socket_state.h"
#include "platform/api/task_runner.h"
#include "platform/api/tls_connection.h"
#include "platform/impl/ssl_context.h"
#include "platform/impl/stream_socket_posix.h"

namespace openscreen {
namespace platform {

class TlsConnectionPosix : public TlsConnection {
 public:
  explicit TlsConnectionPosix(IPEndpoint local_endpoint,
                              TaskRunner* task_runner);
  ~TlsConnectionPosix();

  // TlsConnection overrides
  void Write(const std::vector<uint8_t>& message) override;
  const absl::optional<IPEndpoint>& local_address() const override;
  const absl::optional<IPEndpoint>& remote_address() const override;

 private:
  const absl::optional<IPEndpoint> local_address_;
  const absl::optional<IPEndpoint> remote_address_;

  // TODO(jophba, rwkeane): Set up StreamSocket.
  std::unique_ptr<StreamSocket> socket_;

  // TODO(jophba, rwkeane): Initialize SSL object.
  bssl::UniquePtr<SSL> ssl_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_TLS_CONNECTION_POSIX_H_
