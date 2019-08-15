// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_SOCKET_STATE_H_
#define PLATFORM_API_SOCKET_STATE_H_

#include <cstdint>
#include <memory>
#include <string>

namespace openscreen {
namespace platform {

// SocketState should be used for various socket types (Tls, Tcp) for indicating
// current connection state.
enum class SocketState {
  // Socket has been created, but the internal, lazy initialization has
  // not yet occurred so the socket is not stood up yet.
  kUninitialized = 0,

  // Socket is initialized, but not bound.
  kUnbound,

  // Socket is initialized and bound.
  kOpen,

  // Socket is actively connected to a remote.
  kConnected,

  // Socket Close() has been called, so the socket connected has been
  // terminated.
  kClosed
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_SOCKET_STATE_H_
