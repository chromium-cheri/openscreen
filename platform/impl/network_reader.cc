// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/network_reader.h"

#include <chrono>
#include <condition_variable>

#include "platform/api/logging.h"
#include "platform/impl/udp_socket_posix.h"

namespace openscreen {
namespace platform {

NetworkReader::NetworkReader() : NetworkReader(NetworkWaiter::Create()) {}

NetworkReader::NetworkReader(std::unique_ptr<NetworkWaiter> waiter)
    : waiter_(std::move(waiter)), is_running_(false) {}

NetworkReader::~NetworkReader() = default;

Error NetworkReader::WaitAndRead(Clock::duration timeout) {
  // Get the set of all sockets we care about. A different list than the
  // existing unordered_set is used to avoid race conditions with the method
  // using this new list.
  socket_deletion_block_.notify_all();
  std::vector<UdpSocket*> sockets;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& read : read_sockets_) {
      sockets.push_back(read);
    }
  }

  // Wait for the sockets to find something interesting or for the timeout.
  auto changed_or_error = waiter_->AwaitSocketsReadable(sockets, timeout);
  if (changed_or_error.is_error()) {
    return changed_or_error.error();
  }

  // Process the results.
  socket_deletion_block_.notify_all();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (UdpSocket* read : changed_or_error.value()) {
      if (read_sockets_.find(read) == read_sockets_.end()) {
        continue;
      }

      // TODO(rwkeane): Remove this unsafe cast.
      UdpSocketPosix* read_socket = static_cast<UdpSocketPosix*>(read);
      ErrorOr<UdpPacket> read_packet = read_socket->ReceiveMessage();
      read_socket->OnRead(std::move(read_packet));
    }
  }

  return Error::None();
}

void NetworkReader::RunUntilStopped() {
  const bool was_running = is_running_.exchange(true);
  OSP_CHECK(!was_running);

  Clock::duration timeout = std::chrono::milliseconds(50);
  while (is_running_) {
    WaitAndRead(timeout);
  }
}

void NetworkReader::RequestStopSoon() {
  is_running_.store(false);
}

void NetworkReader::OnCreate(UdpSocket* socket) {
  std::lock_guard<std::mutex> lock(mutex_);
  read_sockets_.insert(socket);
}

void NetworkReader::OnDestroy(UdpSocket* socket) {
  OnDelete(socket, true);
}

void NetworkReader::OnDelete(UdpSocket* socket, bool should_lock) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (read_sockets_.erase(socket) != 0 && should_lock) {
    // This code will allow us to block completion of the socket destructor (and
    // subsequent invalidation of pointers to this socket) until we no longer
    // are waiting on a SELECT(...) call to it, since we only signal this
    // condition variable's wait(...) to proceed outside of SELECT(...).
    socket_deletion_block_.wait(lock);
  }
}

}  // namespace platform
}  // namespace openscreen
