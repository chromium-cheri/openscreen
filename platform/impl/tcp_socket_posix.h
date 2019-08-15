// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_TCP_SOCKET_POSIX_H_
#define PLATFORM_IMPL_TCP_SOCKET_POSIX_H_

#include <atomic>
#include <memory>
#include <string>

#include "absl/types/optional.h"
#include "platform/api/tcp_socket.h"
#include "platform/base/ip_address.h"
#include "platform/impl/socket_address_posix.h"

namespace openscreen {
namespace platform {

class TcpSocketPosix : public TcpSocket {
 public:
  explicit TcpSocketPosix(const IPEndpoint& local_endpoint);
  TcpSocketPosix(SocketAddressPosix address, int file_descriptor);

  // TcpSocketPosix is non-copyable, due to directly managing the file
  // descriptor.
  TcpSocketPosix(const TcpSocketPosix& other) = delete;
  TcpSocketPosix& operator=(const TcpSocketPosix& other) = delete;
  virtual ~TcpSocketPosix();

  // TcpSocket overrides.
  std::unique_ptr<TcpSocket> Accept() override;
  Error Bind() override;
  Error Close() override;
  Error Connect(const IPEndpoint& peer_endpoint) override;
  int64_t GetFileDescriptor() const override;
  Error Listen() override;
  Error Listen(int max_backlog_size) override;

  // Returns the local address.
  const SocketAddressPosix& GetLocalAddress() const;

 private:
  // TcpSocketPosix is lazy initialized on first usage. For simplicitly,
  // the ensure method returns a boolean of whether or not the socket was
  // initialized successfully.
  bool EnsureInitialized();
  Error Initialize();

  Error CloseOnError(Error::Code error_code);
  bool IsOpen();
  Error ReportSocketClosedError();

  const SocketAddressPosix address_;
  std::atomic_int file_descriptor_;

  // last_error_code_ is an Error::Code due to atomic's type requirements.
  std::atomic<Error::Code> last_error_code_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_TCP_SOCKET_POSIX_H_
