// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "platform/api/udp_socket.h"

#include "platform/api/task_runner.h"

namespace openscreen {
namespace platform {

UdpSocket::UdpSocket(TaskRunner* task_runner, Client* client)
    : client_(client), task_runner_(task_runner) {
  OSP_CHECK(task_runner_);
  deletion_callback_ = [](UdpSocket* socket) {};
}

UdpSocket::~UdpSocket() {
  deletion_callback_(this);
}

void UdpSocket::SetDeletionCallback(std::function<void(UdpSocket*)> callback) {
  deletion_callback_ = callback;
}

void UdpSocket::OnError(Error error) {
  if (client_ == nullptr) {
    return;
  }

  auto callback = [e = std::move(error), this]() mutable {
    this->client_->OnError(this, std::move(e));
  };
  task_runner_->PostTask(callback);
}

void UdpSocket::OnSendError(Error error) {
  if (client_ == nullptr) {
    return;
  }

  auto callback = [e = std::move(error), this]() mutable {
    this->client_->OnSendError(this, std::move(e));
  };
  task_runner_->PostTask(callback);
}

void UdpSocket::OnRead(ErrorOr<UdpPacket> read_data) {
  if (client_ == nullptr) {
    return;
  }

  auto callback = [data = std::move(read_data), this]() mutable {
    this->client_->OnRead(this, std::move(data));
  };
  task_runner_->PostTask(std::move(callback));
}

}  // namespace platform
}  // namespace openscreen
