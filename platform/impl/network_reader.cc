// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/network_reader.h"

#include <chrono>
#include <condition_variable>

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {

NetworkReader::NetworkReader() : NetworkReader(NetworkWaiter::Create()) {}

NetworkReader::NetworkReader(std::unique_ptr<NetworkWaiter> waiter)
    : waiter_(std::move(waiter)), is_running_(false) {}

NetworkReader::~NetworkReader() = default;

Error NetworkReader::WatchSocket(UdpSocket* socket) {
  std::lock_guard<std::mutex> lock(mutex_);
  return sockets_.insert(socket).second ? Error::None()
                                        : Error{Error::Code::kAlreadyListening};
}

Error NetworkReader::UnwatchSocket(UdpSocket* socket, bool is_deletion) {
  // TODO(rwkeane): Break out of wait loop here.
  std::unique_lock<std::mutex> lock(mutex_);
  if (sockets_.erase(socket) != 0) {
    // This code will allow us to block completion of the socket destructor (and
    // subsequent invalidation of pointers to this socket) until we no longer
    // are waiting on a SELECT(...) call to it, since we only signal this
    // condition variable's wait(...) to proceed outside of SELECT(...).
    if (is_deletion) {
      socket_deletion_block_.wait(lock);
    }
    return Error::None();
  } else {
    return Error::Code::kNotRunning;
  }
}

Error NetworkReader::WaitAndRead(Clock::duration timeout) {
  // Get the set of all sockets we care about.
  socket_deletion_block_.notify_all();
  std::vector<UdpSocket*> sockets;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& read : sockets_) {
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
  Error error = Error::None();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (UdpSocket* read : changed_or_error.value()) {
      Error result = read->ReceiveMessage();
      if (!result.ok()) {
        error = result;
        continue;
      }
    }
  }

  return error;
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

}  // namespace platform
}  // namespace openscreen
