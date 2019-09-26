// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/network_reader_writer_thread.h"

namespace openscreen {
namespace platform {

NetworkReaderWriterThread::NetworkReaderWriterThread()
    : thread_(&NetworkReaderWriterPosix::RunUntilStopped,
              &network_reader_writer_) {}

NetworkReaderWriterThread::~NetworkReaderWriterThread() {
  network_reader_writer_.RequestStopSoon();
  thread_.join();
}

}  // namespace platform
}  // namespace openscreen
