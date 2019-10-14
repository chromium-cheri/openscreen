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
}

UdpSocket::~UdpSocket() {
  OSP_DCHECK(is_closed_);
}

void UdpSocket::OnError(Error error) {
  CloseIfError(error);

  if (!client_) {
    return;
  }

  task_runner_->PostTask([e = std::move(error), this]() mutable {
    // TODO(issues/71): |this| may be invalid at this point.
    this->client_->OnError(this, std::move(e));
  });
}

void UdpSocket::OnSendError(Error error) {
  if (!client_) {
    return;
  }

  task_runner_->PostTask([e = std::move(error), this]() mutable {
    // TODO(issues/71): |this| may be invalid at this point.
    this->client_->OnSendError(this, std::move(e));
  });
}

void UdpSocket::OnRead(ErrorOr<UdpPacket> read_data) {
  if (!client_) {
    return;
  }

  task_runner_->PostTask([data = std::move(read_data), this]() mutable {
    // TODO(issues/71): |this| may be invalid at this point.
    this->client_->OnRead(this, std::move(data));
  });
}

void UdpSocket::CloseIfError(const Error& error) {
  if (error.code() != Error::Code::kNone &&
      error.code() != Error::Code::kAgain) {
    CloseIfOpen();
  }
}

void UdpSocket::CloseIfOpen() {
  if (!is_closed_.exchange(true)) {
    Close();
  }
}

}  // namespace platform
}  // namespace openscreen
