// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "platform/api/udp_socket.h"

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {

UdpSocket::UdpSocket(const Observer* observer) : observer_(observer) {
  OSP_DCHECK(observer_ != nullptr);
}

UdpSocket::~UdpSocket() {
  observer_->OnDestroyed(this);
}

}  // namespace platform
}  // namespace openscreen
