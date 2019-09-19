// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/tls_networking_manager_posix.h"

#include "absl/algorithm/container.h"
#include "platform/api/logging.h"

namespace openscreen {
namespace platform {

TlsNetworkingManagerPosix::TlsNetworkingManagerPosix(NetworkWaiter* waiter)
    : waiter_(waiter) {}

TlsNetworkingManagerPosix::~TlsNetworkingManagerPosix() {
  waiter_->DeregisterAll(this);
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

void TlsNetworkingManagerPosix::RegisterListener(
    StreamSocketPosix* socket,
    TlsConnectionFactory::Client* client) {
  OSP_DCHECK(socket);
  OSP_DCHECK(client);
  std::unique_lock<std::mutex> lock(socket_mutex_);
  if (socket_mappings_.find(socket) == socket_mappings_.end()) {
    socket_mappings_.emplace(socket, client);
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
  {
    std::unique_lock<std::mutex> lock(socket_mutex_);
    auto it = socket_mappings_.find(socket);
    if (it != socket_mappings_.end()) {
      socket_mappings_.erase(it);
    }
  }

  waiter_->OnHandleDeletion(this, std::cref(socket->socket_handle()),
                            skip_locking_for_testing);
}

void TlsNetworkingManagerPosix::ProcessReadyHandle(
    NetworkWaiter::SocketHandleRef handle) {
  std::unique_lock<std::mutex> lock(socket_mutex_);
  for (const auto& pair : socket_mappings_) {
    if (pair->first->socket_handle() == handle) {
      ErrorOr<std::unique_ptr<StreamSocket>> result =
          std::move(pair->first->Accept());
      if (result.IsError()) {
        pair->second->OnError(pair.error());
      } else {
        pair->second->OnAccepted(std::move(pair.value()));
      }
      break;
    }
  }
}

}  // namespace platform
}  // namespace openscreen
