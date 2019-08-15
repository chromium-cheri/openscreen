// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_STREAM_SOCKET_POSIX_H_
#define PLATFORM_IMPL_STREAM_SOCKET_POSIX_H_

#include <string>

#include "platform/base/ip_address.h"
#include "platform/impl/socket_address_posix.h"

namespace openscreen {
namespace platform {

class StreamSocketPosix {
 public:
  StreamSocketPosix(const IPEndpoint& local_endpoint);
  StreamSocketPosix() = default;

  // Used by passive/server sockets to accept connection requests
  // from a client.
  ErrorOr<StreamSocketPosix> Accept(const IPEndpoint& peer_endpoint);

  // Bind to the address given in the constructor.
  Error Bind();

  // Closes the socket.
  Error Close();

  // Connects the socket to a specified remote address.
  Error Connect(const IPEndpoint& peer_endpoint);

  // Marks the socket as passive, to receive incoming connections.
  Error Listen();

  int file_descriptor() { return file_descriptor_; }
  bool in_valid_state() { return in_valid_state_; }

 private:
  StreamSocketPosix(int file_descriptor, SocketAddressPosix address);

  // Address may be null if not in valid state.
  std::unique_ptr<SocketAddressPosix> address_;
  int file_descriptor_ = -1;
  bool in_valid_state_ = true;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_STREAM_SOCKET_POSIX_H_
