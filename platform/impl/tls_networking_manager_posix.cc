// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/tls_networking_manager_posix.h"

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {

TlsNetworkingManagerPosix::TlsNetworkingManagerPosix(NetworkWaiter* waiter)
    : waiter_(waiter) {}

void TlsNetworkingManagerPosix::RegisterConnection(
    TlsConnectionPosix* connection) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}

void TlsNetworkingManagerPosix::DeregisterConnection(
    TlsConnectionPosix* connection) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}

void TlsNetworkingManagerPosix::RegisterListener(
    StreamSocketPosix* socket,
    TlsConnectionFactory::Client* client) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}

void TlsNetworkingManagerPosix::OnConnectionDestroyed(
    TlsConnectionPosix* connection) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}

void TlsNetworkingManagerPosix::OnSocketDestroyed(StreamSocketPosix* socket) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}

void TlsNetworkingManagerPosix::ProcessReadyHandle(
    NetworkWaiter::SocketHandleRef handle) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}

}  // namespace platform
}  // namespace openscreen
