// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_TLS_SOCKET_POSIX_H_
#define PLATFORM_IMPL_TLS_SOCKET_POSIX_H_

#include <memory>
#include <string>

#include "platform/api/socket_state.h"
#include "platform/api/tls_socket.h"
#include "platform/impl/ssl_context.h"
#include "platform/impl/stream_socket_posix.h"

namespace openscreen {
namespace platform {

class TlsSocketPosix : public TlsSocket {
 public:
  TlsSocketPosix(Client* client,
                 const std::string& parent_id,
                 IPEndpoint local_endpoint);
  ~TlsSocketPosix();

  // TlsSocket overrides
  bool IsIPv4() const override;
  bool IsIPv6() const override;
  void Close(CloseReason reason) override;
  void Write(const TlsPacket& message) override;
  SocketState state() const override;
  absl::optional<std::string> parent_server_socket_id() const override;
  const std::string& id() const override;

 private:
  const std::string id_;
  const absl::optional<std::string> parent_id_;
  std::unique_ptr<StreamSocket> socket_;
  bssl::UniquePtr<SSL> ssl_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_TLS_SOCKET_POSIX_H_
