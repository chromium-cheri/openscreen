// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_TLS_NETWORKING_MANAGER_POSIX_H_
#define PLATFORM_IMPL_TLS_NETWORKING_MANAGER_POSIX_H_

#include "platform/impl/network_waiter.h"
#include "platform/impl/stream_socket_posix.h"
#include "platform/impl/tls_connection_factory_posix.h"
#include "platform/impl/tls_connection_posix.h"

namespace openscreen {
namespace platform {

class TlsNetworkingManagerPosix : public NetworkWaiter::Subscriber {
 public:
  TlsNetworkingManagerPosix(NetworkWaiter* waiter);

  // Register a TlsConnection that should be watched for readable and writable
  // data.
  void RegisterConnection(TlsConnectionPosix* connection);

  // Deregister a TlsConnection.
  void DeregisterConnection(TlsConnectionPosix* connection);

  // Register a StreamSocket that should be watched for income Tcp Connections.
  void RegisterListener(StreamSocketPosix* socket,
                        TlsConnectionFactory::Listener* listener);

  // Method to be executed on TlsConnection destruction.
  void OnConnectionDestroyed(TlsConnectionPosix* connection);

  // Method to be executed on StreamSocket destruction.
  void OnSocketDestroyed(StreamSocketPosix* socket);

  // NetworkWaiter::Subscriber overrides.
  void ProcessReadyHandle(NetworkWaiter::SocketHandleRef handle) override;

 private:
  NetworkWaiter* waiter_;
}

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_TLS_NETWORKING_MANAGER_POSIX_H_
