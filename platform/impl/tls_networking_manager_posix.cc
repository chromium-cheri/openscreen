// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/tls_networking_manager_posix.h"

#include "absl/algorithm/container.h"
#include "platform/api/logging.h"
#include "platform/impl/stream_socket_posix.h"
#include "platform/impl/tls_connection_posix.h"

namespace openscreen {
namespace platform {

TlsNetworkingManagerPosix::TlsNetworkingManagerPosix(NetworkWaiter* waiter)
    : waiter_(waiter) {}

TlsNetworkingManagerPosix::~TlsNetworkingManagerPosix() {
  waiter_->UnsubscribeAll(this);
}

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

void TlsNetworkingManagerPosix::RegisterListener(StreamSocketPosix* socket,
                                                 SocketObserver* observer) {
  OSP_DCHECK(socket);
  OSP_DCHECK(observer);
  std::unique_lock<std::mutex> lock(socket_mutex_);
  if (socket_mappings_.find(socket) == socket_mappings_.end()) {
    socket_mappings_.emplace(socket, observer);
  }
}

void TlsNetworkingManagerPosix::DeregisterListener(StreamSocketPosix* socket) {
  std::unique_lock<std::mutex> lock(socket_mutex_);
  auto it = socket_mappings_.find(socket);
  if (it != socket_mappings_.end()) {
    socket_mappings_.erase(it);
  }
}

void TlsNetworkingManagerPosix::OnConnectionDestroyed(
    TlsConnectionPosix* connection) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}

void TlsNetworkingManagerPosix::OnSocketDestroyed(StreamSocketPosix* socket) {
  OnSocketDestroyed(socket, false);
}

void TlsNetworkingManagerPosix::OnSocketDestroyed(
    StreamSocketPosix* socket,
    bool skip_locking_for_testing) {
  DeregisterListener(socket);
  waiter_->OnHandleDeletion(this, std::cref(socket->socket_handle()),
                            skip_locking_for_testing);
}

void TlsNetworkingManagerPosix::ProcessReadyHandle(
    NetworkWaiter::SocketHandleRef handle) {
  std::unique_lock<std::mutex> lock(socket_mutex_);
  for (const auto& pair : socket_mappings_) {
    if (pair.first->socket_handle() == handle) {
      pair.second->OnConnectionPending(pair.first);
      break;
    }
  }
}

bool TlsNetworkingManagerPosix::IsSocketMapped(
    StreamSocketPosix* socket) const {
  return socket_mappings_.find(socket) != socket_mappings_.end();
}

}  // namespace platform
}  // namespace openscreen
