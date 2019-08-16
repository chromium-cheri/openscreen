// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "platform/api/udp_socket.h"

namespace openscreen {
namespace platform {

UdpSocket::UdpSocket() {
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

}  // namespace platform
}  // namespace openscreen
