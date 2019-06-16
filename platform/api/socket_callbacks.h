// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef PLATFORM_API_SOCKET_CALLBACKS_H_
#define PLATFORM_API_SOCKET_CALLBACKS_H_

#include <array>
#include <cstdint>
#include <memory>

#include "osp_base/ip_address.h"

namespace openscreen {
namespace platform {

class NetworkRunner;
class Socket;

static constexpr int kMaxSocketPacketSize = 1 << 16;

// TODO(jophba): unify with delegate interface in Socket.
class SocketReadCallback {
 public:
  virtual ~SocketReadCallback() = default;

  virtual void OnRead(std::unique_ptr<Socket::Message> data,
                      NetworkRunner* network_runner) = 0;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_SOCKET_CALLBACKS_H_
