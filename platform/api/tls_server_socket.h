// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TLS_SERVER_SOCKET_H_
#define PLATFORM_API_TLS_SERVER_SOCKET_H_

#include <memory>
#include <string>

#include "absl/types/optional.h"
#include "platform/api/tls_socket.h"
#include "platform/base/ip_address.h"

namespace openscreen {
namespace platform {

class TlsServerSocket {
 public:
  // Returns a unique identifier for this instance.
  virtual const std::string& id() const = 0;

  // Gets the local address, if set.
  virtual const absl::optional<IPEndpoint>& local_address() const = 0;

  // Start accepting new sockets. Should call Client::OnAccepted().
  virtual void Accept() = 0;

  // Stop accepting new sockets.
  virtual void Stop() = 0;

  // Set the credentials used for communication.
  virtual void SetCredentials(TlsSocketCreds creds) = 0;

 protected:
  TlsServerSocket(TlsSocket::Client* client) : client_(client) {}

  TlsSocket::Client* client() const { return client_; }

 private:
  TlsSocket::Client* client_;

  OSP_DISALLOW_COPY_AND_ASSIGN(TlsServerSocket);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_TLS_SERVER_SOCKET_H_
