// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_STREAM_SOCKET_POSIX_H_
#define PLATFORM_IMPL_STREAM_SOCKET_POSIX_H_

#include <memory>
#include <string>

#include "absl/types/optional.h"
#include "platform/base/ip_address.h"
#include "platform/impl/socket_address_posix.h"

namespace openscreen {
namespace platform {

class StreamSocketPosix {
 public:
  explicit StreamSocketPosix(const IPEndpoint& local_endpoint);
  StreamSocketPosix(SocketAddressPosix address, int file_descriptor);

  // We cannot be copied, since we manage the file descriptor directly.
  StreamSocketPosix() = delete;
  StreamSocketPosix(const StreamSocketPosix& other) = delete;
  StreamSocketPosix& operator=(const StreamSocketPosix& other) = delete;
  ~StreamSocketPosix();

  Error Initialize();

  // Used by passive/server sockets to accept connection requests
  // from a client. If no socket is available, this method returns nullptr.
  // To get more information, the client may call last_error().
  std::unique_ptr<StreamSocketPosix> Accept();

  // Bind to the address given in the constructor.
  Error Bind();

  // Closes the socket.
  Error Close();

  // Connects the socket to a specified remote address.
  Error Connect(const IPEndpoint& peer_endpoint);

  // Marks the socket as passive, to receive incoming connections.
  Error Listen();
  Error Listen(int max_backlog_size);

  const absl::optional<SocketAddressPosix>& address() const { return address_; }
  int file_descriptor() const { return file_descriptor_; }
  Error last_error() const { return last_error_code_.load(); }

 private:
  Error CloseOnError(Error::Code error_code);
  Error ReportSocketClosedError();

  // Address may be empty if not in valid state.
  absl::optional<SocketAddressPosix> address_;

  int file_descriptor_ = -1;
  bool is_open_ = false;

  // last_error_code_ is an Error::Code due to atomic requirements. This is done
  // to avoid race issues caused by potential multithreading scenarios.
  std::atomic<Error::Code> last_error_code_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_STREAM_SOCKET_POSIX_H_
