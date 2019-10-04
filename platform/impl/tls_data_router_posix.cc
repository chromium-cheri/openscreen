// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/tls_data_router_posix.h"

#include "platform/api/logging.h"
#include "platform/impl/stream_socket_posix.h"
#include "platform/impl/tls_connection_posix.h"

namespace openscreen {
namespace platform {

TlsDataRouterPosix::TlsDataRouterPosix(SocketHandleWaiter* waiter)
    : waiter_(waiter) {}

TlsDataRouterPosix::~TlsDataRouterPosix() {
  waiter_->UnsubscribeAll(this);
}

void TlsDataRouterPosix::RegisterConnection(TlsConnectionPosix* connection) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}

void TlsDataRouterPosix::DeregisterConnection(TlsConnectionPosix* connection) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}

void TlsDataRouterPosix::RegisterSocketObserver(StreamSocketPosix* socket,
                                                SocketObserver* observer) {
  OSP_DCHECK(socket);
  OSP_DCHECK(observer);
  {
    std::unique_lock<std::mutex> lock(socket_mutex_);
    socket_mappings_[socket] = observer;
  }

  waiter_->Subscribe(this, socket->socket_handle());
}

void TlsDataRouterPosix::DeregisterSocketObserver(StreamSocketPosix* socket) {
  RemoveWatchedSocket(socket);
  waiter_->Unsubscribe(this, socket->socket_handle());
}

void TlsDataRouterPosix::OnConnectionDestroyed(TlsConnectionPosix* connection) {
  // TODO(jophba, rwkeane): implement this method.
  OSP_UNIMPLEMENTED();
}

void TlsDataRouterPosix::OnSocketDestroyed(StreamSocketPosix* socket) {
  OnSocketDestroyed(socket, false);
}

void TlsDataRouterPosix::OnSocketDestroyed(StreamSocketPosix* socket,
                                           bool skip_locking_for_testing) {
  RemoveWatchedSocket(socket);
  waiter_->OnHandleDeletion(this, std::cref(socket->socket_handle()),
                            skip_locking_for_testing);
}

void TlsDataRouterPosix::ProcessReadyHandle(
    SocketHandleWaiter::SocketHandleRef handle) {
  std::unique_lock<std::mutex> lock(socket_mutex_);
  for (const auto& pair : socket_mappings_) {
    if (pair.first->socket_handle() == handle) {
      pair.second->OnConnectionPending(pair.first);
      break;
    }
  }
}

void TlsDataRouterPosix::PerformNetworkingOperations(Clock::duration timeout) {
  Clock::time_point start_time = Clock::now();
  auto* start_connection = last_connection_processed_;
  auto start_mode = last_state_;

  // TODO(rwkeane): Minimize time locked based on how RegisterConnection and
  // DeregisterConnection are implimented.
  std::lock_guard<std::mutex> lock(connections_mutex_);
  if (start_connection == nullptr) {
    // TODO(rwkeane): Update this logic to align with RegisterConnection and
    // DeregisterConnection.
    if (!connections_.empty()) {
      last_connection_processed_ = connections_[0];
      start_connection = last_connection_processed_;
    } else {
      return;
    }
  }

  auto current_mode = start_mode;
  auto* current_connection = start_connection;
  do {
    // Get the next (connection, mode) pair to use for processing.
    current_mode = GetNextMode(current_mode);
    if (current_mode == NetworkingOperation::kReading) {
      current_connection = GetNextConnection(current_connection);
    }

    // Process the (connection, mode).
    switch (current_mode) {
      case NetworkingOperation::kReading:
        current_connection->TryReceiveMessage();
        break;
      case NetworkingOperation::kWriting:
        current_connection->SendAvailableBytes();
        break;
    }

    last_connection_processed_ = current_connection;
    last_state_ = current_mode;

    // If this (connection, mode) is where we started, exit.
    if (start_connection == current_connection && start_mode == current_mode) {
      break;
    }
  } while (!HasTimedOut(start_time, timeout));
}

bool TlsDataRouterPosix::HasTimedOut(Clock::time_point start_time,
                                     Clock::duration timeout) {
  return Clock::now() - start_time > timeout;
}

void TlsDataRouterPosix::RemoveWatchedSocket(StreamSocketPosix* socket) {
  std::unique_lock<std::mutex> lock(socket_mutex_);
  const auto it = socket_mappings_.find(socket);
  if (it != socket_mappings_.end()) {
    socket_mappings_.erase(it);
  }
}

bool TlsDataRouterPosix::IsSocketWatched(StreamSocketPosix* socket) const {
  std::unique_lock<std::mutex> lock(socket_mutex_);
  return socket_mappings_.find(socket) != socket_mappings_.end();
}

TlsDataRouterPosix::NetworkingOperation TlsDataRouterPosix::GetNextMode(
    NetworkingOperation state) {
  return state == NetworkingOperation::kReading ? NetworkingOperation::kWriting
                                                : NetworkingOperation::kReading;
}

TlsConnectionPosix* TlsDataRouterPosix::GetNextConnection(
    TlsConnectionPosix* current) {
  auto current_pos =
      std::find(connections_.begin(), connections_.end(), current);
  if (current_pos == connections_.end() ||
      ++current_pos == connections_.end()) {
    return *connections_.begin();
  }

  return *current_pos;
}

}  // namespace platform
}  // namespace openscreen
