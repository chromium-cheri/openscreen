// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "platform/api/udp_socket.h"

#include "platform/api/task_runner.h"

namespace openscreen {
namespace platform {

UdpSocket::UdpSocket(TaskRunner* task_runner, Client* client, bool is_reading)
    : client_(client), task_runner_(task_runner), is_reading_(is_reading) {
  OSP_CHECK(task_runner_);
  if (lifetime_observer_) {
    std::lock_guard<std::mutex> lock(lifetime_observer_mutex_);
    if (lifetime_observer_) {
      lifetime_observer_->OnCreate(this);
    }
  }
}

UdpSocket::~UdpSocket() {
  if (lifetime_observer_) {
    std::lock_guard<std::mutex> lock(lifetime_observer_mutex_);
    if (lifetime_observer_) {
      lifetime_observer_->OnDestroy(this);
    }
  }
}

// static
void UdpSocket::SetLifetimeObserver(LifetimeObserver* observer) {
  std::lock_guard<std::mutex> lock(lifetime_observer_mutex_);
  lifetime_observer_ = observer;
}

// static
UdpSocket::LifetimeObserver* UdpSocket::lifetime_observer_ = nullptr;

// static
std::mutex UdpSocket::lifetime_observer_mutex_;

void UdpSocket::OnError(Error error) {
  if (!client_) {
    return;
  }

  task_runner_->PostTask([e = std::move(error), this]() mutable {
    this->client_->OnError(this, std::move(e));
  });
}
void UdpSocket::OnSendError(Error error) {
  if (!client_) {
    return;
  }

  task_runner_->PostTask([e = std::move(error), this]() mutable {
    this->client_->OnSendError(this, std::move(e));
  });
}
void UdpSocket::OnRead(ErrorOr<UdpPacket> read_data) {
  if (!is_reading_ || !client_) {
    return;
  }

  task_runner_->PostTask([data = std::move(read_data), this]() mutable {
    this->client_->OnRead(this, std::move(data));
  });
}

}  // namespace platform
}  // namespace openscreen
