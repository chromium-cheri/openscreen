// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "platform/api/udp_socket.h"

#include "platform/api/runtime_context.h"
#include "platform/api/task_runner.h"

namespace openscreen {
namespace platform {

UdpSocket::UdpSocket(RuntimeContext* runtime_context, Client* client)
    : client_(client), runtime_context_(runtime_context) {
  OSP_CHECK(runtime_context_);
  if (lifetime_observer_.load()) {
    lifetime_observer_.load()->OnCreate(this);
  }
}

UdpSocket::~UdpSocket() {
  OSP_DCHECK(is_closed_);
}

// static
void UdpSocket::SetLifetimeObserver(LifetimeObserver* observer) {
  lifetime_observer_.store(observer);
}

// static
std::atomic<UdpSocket::LifetimeObserver*> UdpSocket::lifetime_observer_{
    nullptr};

void UdpSocket::OnError(Error error) {
  CloseIfError(error);

  if (!client_) {
    return;
  }

  runtime_context_->task_runner()->PostTask(
      [e = std::move(error), this]() mutable {
        // TODO(issues/71): |this| may be invalid at this point.
        this->client_->OnError(this, std::move(e));
      });
}

void UdpSocket::OnSendError(Error error) {
  if (!client_) {
    return;
  }

  runtime_context_->task_runner()->PostTask(
      [e = std::move(error), this]() mutable {
        // TODO(issues/71): |this| may be invalid at this point.
        this->client_->OnSendError(this, std::move(e));
      });
}

void UdpSocket::OnRead(ErrorOr<UdpPacket> read_data) {
  if (!client_) {
    return;
  }

  runtime_context_->task_runner()->PostTask(
      [data = std::move(read_data), this]() mutable {
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
    if (lifetime_observer_.load()) {
      lifetime_observer_.load()->OnDestroy(this);
    }
    Close();
  }
}

}  // namespace platform
}  // namespace openscreen
