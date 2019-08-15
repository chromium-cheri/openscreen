// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TCP_SOCKET_H_
#define PLATFORM_API_TCP_SOCKET_H_

#include <cstdint>
#include <memory>
#include <string>

#include "platform/api/network_interface.h"
#include "platform/api/tls_socket_creds.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"
#include "platform/base/macros.h"

namespace openscreen {
namespace platform {

// TcpSocket represents an incomplete abstraction of a TCP socket, with the
// intention of the platform providing all of the platform specific methods
// not handled by BoringSSL.
class TcpSocket {
 public:
  enum class State { kUninitialized, kOpen, kConnected, kClosed };

  TcpSocket() = default;
  virtual ~TcpSocket() = default;

  // Used by passive/server sockets to accept connection requests
  // from a client. If no socket is available, this method returns nullptr.
  // To get more information, the client may call last_error().
  virtual std::unique_ptr<TcpSocket> Accept() = 0;

  // Bind to the address given upon construction.
  virtual Error Bind() = 0;

  // Closes the socket.
  virtual Error Close() = 0;

  // Connects the socket to a specified remote address.
  virtual Error Connect(const IPEndpoint& peer_endpoint) = 0;

  // Returns the file descriptor (e.g. fd or HANDLE pointer) for this socket.
  virtual int64_t GetFileDescriptor() const = 0;

  // Returns the connected peer address, if socket is connected.
  virtual ErrorOr<IPEndpoint> GetPeerAddress() const = 0;

  // Returns the state of the socket.
  virtual State GetState() const = 0;

  // Marks the socket as passive, to receive incoming connections.
  virtual Error Listen() = 0;
  virtual Error Listen(int max_backlog_size) = 0;

 protected:
  OSP_DISALLOW_COPY_AND_ASSIGN(TcpSocket);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_TCP_SOCKET_H_
