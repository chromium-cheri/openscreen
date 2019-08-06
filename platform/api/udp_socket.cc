// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "platform/api/udp_socket.h"

#include "platform/api/network_runner.h"
#include "platform/api/udp_packet.h"
#include "platform/api/udp_read_callback.h"

namespace openscreen {
namespace platform {

UdpSocket::UdpSocket(NetworkRunner* network_runner)
    : network_runner_(network_runner) {
  network_runner_->OnSocketCreation(this);
}

UdpSocket::~UdpSocket() {
  network_runner_->OnSocketDeletion(this);
}

void UdpSocket::PostReadData(UdpPacket packet) {
  if (!read_callback_) {
    return;
  }

  auto callback = [p = std::move(packet), this]() mutable {
    this->read_callback_->OnRead(std::move(p), this->network_runner_);
  };
  network_runner_->PostTask(std::move(callback));
}

Error UdpSocket::set_read_callback(UdpReadCallback* callback) {
  if (read_callback_) {
    return Error::Code::kIOFailure;
  }

  read_callback_ = callback;
  return Error::Code::kNone;
}

Error UdpSocket::clear_read_callback() {
  if (!read_callback_) {
    return Error::Code::kNotRunning;
  }

  read_callback_ = nullptr;
  return Error::Code::kNone;
}

}  // namespace platform
}  // namespace openscreen
