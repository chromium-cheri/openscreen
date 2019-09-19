// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_TLS_NETWORKING_MANAGER_POSIX_H_
#define PLATFORM_IMPL_TLS_NETWORKING_MANAGER_POSIX_H_

#include <map>
#include <mutex>

#include "platform/impl/network_waiter.h"

namespace openscreen {
namespace platform {

class TlsConnectionPosix;
class StreamSocketPosix;

// This class is responsible for 3 operations:
//   1) Register StreamSockets with the NetworkWaiter and spin up new
//      TlsConnections when it informs this class of changes.
//   2) Check all registered TlsConnections for read data via boringSSL call
//      and pass all read data to the connection's observer.
//   3) Check all registered TlsConnections' write buffers for additional data
//      to be written out. If any is present, write it using boringSSL.
// These operations will be called repeatedly on the networking thread, so none
// of them should block. Additionally, this class must ensure that deletions of
// the above types do not occur while a socket/connection is currently being
// accessed from the networking thread.
class TlsNetworkingManagerPosix {
 public:
  class SocketObserver {
    virtual ~SocketObserver() = default;
  };

  // Register a TlsConnection that should be watched for readable and writable
  // data.
  void RegisterConnection(TlsConnectionPosix* connection);

  // Deregister a TlsConnection.
  void DeregisterConnection(TlsConnectionPosix* connection);

  // Register a StreamSocket that should be watched for incoming Tcp Connections
  // with the NetworkWaiter.
  void RegisterListener(StreamSocketPosix* socket, SocketObserver* observer);

  // Method to be executed on TlsConnection destruction. This is expected to
  // block until the networking thread is not using the provided connection.
  void OnConnectionDestroyed(TlsConnectionPosix* connection);

  // Method to be executed on StreamSocket destruction. This is expected to
  // block until the networking thread is not using the provided socket.
  void OnSocketDestroyed(StreamSocketPosix* socket);

  // NetworkWaiter::Subscriber overrides.
  // void ProcessReadyHandle(NetworkWaiter::SocketHandleRef handle) override;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_TLS_NETWORKING_MANAGER_POSIX_H_
