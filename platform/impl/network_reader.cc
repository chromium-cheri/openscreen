// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/network_reader.h"

#include <chrono>
#include <condition_variable>
#include <functional>

#include "platform/api/logging.h"
#include "platform/impl/socket_handle_posix.h"
#include "platform/impl/udp_socket_posix.h"

namespace openscreen {
namespace platform {

NetworkReader::NetworkReader(NetworkWaiter* waiter) : waiter_(waiter) {
  waiter_->Subscribe(this);
}

NetworkReader::~NetworkReader() {
  waiter_->Unsubscribe(this);
}

std::vector<NetworkReader::SocketHandleRef> NetworkReader::GetHandles() {
  socket_deletion_block_.notify_all();
  std::vector<SocketHandleRef> socket_handles;
  socket_handles.reserve(sockets_.size());

  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& socket : sockets_) {
      UdpSocketPosix* read_socket = static_cast<UdpSocketPosix*>(socket);
      socket_handles.push_back(std::cref(read_socket->GetHandle()));
    }
  }

  return socket_handles;
}

void NetworkReader::ProcessReadyHandle(SocketHandleRef handle) {
  socket_deletion_block_.notify_all();

  std::lock_guard<std::mutex> lock(mutex_);
  // NOTE: Because sockets_ is expected to remain small, the perfomance here
  // is better than using an unordered_set.
  for (UdpSocket* socket : sockets_) {
    UdpSocketPosix* read_socket = static_cast<UdpSocketPosix*>(socket);
    if (read_socket->GetHandle() == handle) {
      read_socket->ReceiveMessage();
      break;
    }
  }
}

void NetworkReader::OnCreate(UdpSocket* socket) {
  std::lock_guard<std::mutex> lock(mutex_);
  sockets_.push_back(socket);
}

void NetworkReader::OnDestroy(UdpSocket* socket) {
  OnDelete(socket);
}

void NetworkReader::OnDelete(UdpSocket* socket,
                             bool disable_locking_for_testing) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto it = std::find(sockets_.begin(), sockets_.end(), socket);
  if (it != sockets_.end()) {
    sockets_.erase(it);
    if (!disable_locking_for_testing) {
      // This code will allow us to block completion of the socket destructor
      // (and subsequent invalidation of pointers to this socket) until we no
      // longer are waiting on a SELECT(...) call to it, since we only signal
      // this condition variable's wait(...) to proceed outside of SELECT(...).
      socket_deletion_block_.wait(lock);
    }
  }
}

}  // namespace platform
}  // namespace openscreen
