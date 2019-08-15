// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_TLS_SOCKET_POSIX_H_
#define PLATFORM_IMPL_TLS_SOCKET_POSIX_H_

#include <string>

#include "platform/api/tls_socket.h"
#include "platform/impl/stream_socket_posix.h"

namespace openscreen {
namespace platform {

class TlsSocketPosix : public TlsSocket {
 public:
  // TODO(jophba): constructor?
  // TODO(jophba): SSLifying with boring SSL
  TlsSocketPosix(Client* client, const std::string& parent_id);
  ~TlsSocketPosix() override;

  // Tls socket overrides
  bool IsIPv4() const override;
  bool IsIPv6() const override;
  void Close(CloseReason reason) override;
  void Write(const TlsSocketMessage& message) override;

 private:
  StreamSocketPosix stream_socket_;
};

class TlsServerSocketPosix : public TlsServerSocket {
 public:
  TlsServerSocketPosix() = default;
  ~TlsServerSocketPosix() override = default;

  const std::string& GetId() const override;
  const absl::optional<IPEndpoint>& GetLocalAddress() const override;

  void Accept() override;
  void Stop() override;
  void SetCredentials(TlsSocketCreds creds) override;

 protected:
  TlsSocket::Client* GetClient() const override;

 private:
  absl::optional<IPEndpoint> local_address_;
  TlsSocketCreds socket_credentials_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_TLS_SOCKET_POSIX_H_
