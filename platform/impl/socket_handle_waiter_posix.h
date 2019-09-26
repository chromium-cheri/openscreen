// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_SOCKET_HANDLE_WAITER_POSIX_H_
#define PLATFORM_IMPL_SOCKET_HANDLE_WAITER_POSIX_H_

#include <sys/select.h>
#include <unistd.h>

#include <atomic>
#include <mutex>  // NOLINT

#include "platform/impl/network_reader_writer_posix.h"
#include "platform/impl/socket_handle_waiter.h"

namespace openscreen {
namespace platform {

class SocketHandleWaiterPosix : public SocketHandleWaiter,
                                public NetworkReaderWriterPosix::Provider {
 public:
  using SocketHandleRef = SocketHandleWaiter::SocketHandleRef;

  SocketHandleWaiterPosix(NetworkReaderWriterPosix* network_reader_writer);
  ~SocketHandleWaiterPosix() override;

  // TODO(rwkeane): Move this to a platform-specific util library.
  static struct timeval ToTimeval(const Clock::duration& timeout);

  // NetworkReaderWriterPosix::Provider overrides.
  void PerformNetworkingOperations() override;

 protected:
  ErrorOr<std::vector<SocketHandleRef>> AwaitSocketsReadable(
      const std::vector<SocketHandleRef>& socket_fds,
      const Clock::duration& timeout) override;

 private:
  fd_set read_handles_;

  NetworkReaderWriterPosix* network_reader_writer_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_SOCKET_HANDLE_WAITER_POSIX_H_
