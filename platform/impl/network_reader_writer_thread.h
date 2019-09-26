// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_NETWORK_READER_WRITER_THREAD_H_
#define PLATFORM_IMPL_NETWORK_READER_WRITER_THREAD_H_

#include <thread>

#include "platform/base/macros.h"
#include "platform/impl/network_reader_writer_posix.h"

namespace openscreen {
namespace platform {

// This is the class responsible for handling the threading associated with
// a socket handle waiter. More specifically, when this object is created, it
// starts a thread on which NetworkWaiter's RunUntilStopped method is called,
// and upon destruction it calls NetworkWaiter's RequestStopSoon method and
// joins the thread it created, blocking until the NetworkWaiter's operation
// completes.
// TODO(rwkeane): Move this class to osp/demo.
class NetworkReaderWriterThread {
 public:
  NetworkReaderWriterThread();
  ~NetworkReaderWriterThread();

  NetworkReaderWriterPosix* network_reader_writer() {
    return &network_reader_writer_;
  }

  OSP_DISALLOW_COPY_AND_ASSIGN(NetworkReaderWriterThread);

 private:
  NetworkReaderWriterPosix network_reader_writer_;
  std::thread thread_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_NETWORK_READER_WRITER_THREAD_H_
