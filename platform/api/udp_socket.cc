// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "platform/api/udp_socket.h"

#include "platform/api/network_runner.h"

namespace openscreen {
namespace platform {

UdpSocket::UdpSocket(NetworkRunner* network_runner)
    : network_runner_(network_runner) {
  deletion_callback_ = [](UdpSocket* socket) {};
}

UdpSocket::~UdpSocket() {
  deletion_callback_(this);
}

void UdpSocket::SetDeletionCallback(std::function<void(UdpSocket*)> callback) {
  deletion_callback_ = callback;
}

std::future<void> UdpSocket::PostCallback(Callback callback, Error result) {
  std::promise<void> promise;
  std::future<void> future = promise.get_future();
  network_runner_->PostTask(
      [this, &callback, &result, p = std::move(promise)]() mutable {
        callback(result, this);
        p.set_value();
      });
  return future;
}

}  // namespace platform
}  // namespace openscreen
