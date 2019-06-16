// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TLS_SERVER_SOCKET_H_
#define PLATFORM_API_TLS_SERVER_SOCKET_H_

#include <cstdint>
#include <memory>
#include <string>

#include "osp_base/error.h"
#include "osp_base/ip_address.h"
#include "osp_base/macros.h"
#include "platform/api/network_interface.h"
#include "platform/api/socket.h"
#include "platform/api/tls_socket_creds.h"

namespace openscreen {
namespace platform {

// Parent class for building tls-type Sockets.
class TlsServerSocket {
 public:
  TlsServerSocket() = default;
  ~TlsServerSocket() = default;

  // Returns a unique identifier for this Factory.
  const std::string& GetId();

  // Gets the local address, if set, otherwise nullptr.
  IPEndpoint* GetLocalAddress();

  // Start accepting new sockets. Should call Delegate::OnAccepted().
  void Accept();

  // Stop accepting new sockets.
  void Stop();

  // Set the credentials used for communication.
  void SetCredentials(const TlsSocketCreds& creds);

 private:
  Socket::Delegate* delegate_;

  OSP_DISALLOW_COPY_AND_ASSIGN(TlsServerSocket);
};
}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_TLS_SERVER_SOCKET_H_
