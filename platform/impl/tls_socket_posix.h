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

class TlsSocketPosix : public TlsSocket, public StreamSocketPosix {
 public:
  // TODO(jophba): SSLifying with boring SSL
  TlsSocketPosix(Client* client,
                 const std::string& parent_id,
                 IPEndpoint local_endpoint);
  ~TlsSocketPosix();

  // Tls socket overrides
  bool IsIPv4() const override;
  bool IsIPv6() const override;
  void Close(CloseReason reason) override;
  void Write(const TlsSocketMessage& message) override;

  const std::string& parent_id() const override;
  const std::string& id() const override;

 private:
  Client* const client_;
  const std::string id_;
  const std::string& parent_id_;
};

class TlsServerSocketPosix : public TlsSocketPosix {
 public:
  int Listen(const IPEndpoint& address);
  int GetLocalAddress(IPEndpoint* address) const;
  int Accept(std::unique_ptr<StreamSocketPosix>* socket);
};

class TlsSocketFactory : public TlsServerSocket {
 public:
  TlsSocketFactory(const std::string& id, int port);
  ~TlsSocketFactory();

  const std::string& GetId() const override;
  const absl::optional<IPEndpoint>& GetLocalAddress() const override;

  void Accept() override;
  void Stop() override;
  void SetCredentials(TlsSocketCreds creds) override;

 protected:
  TlsSocket::Client* GetClient() const override;

 private:
  TlsSocket::Client* client_;
  std::string id_;
  std::unique_ptr<TlsSocketPosix> listen_socket_;
  int port_;
  TlsSocketCreds socket_credentials_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_TLS_SOCKET_POSIX_H_
