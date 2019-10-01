// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_NETWORK_READER_WRITER_H_
#define PLATFORM_IMPL_NETWORK_READER_WRITER_H_

#include "platform/impl/network_reader_writer_posix.h"

#include <algorithm>

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {

NetworkReaderWriterPosix::NetworkReaderWriterPosix(
    std::vector<Provider*> providers)
    : providers_(providers) {}

void NetworkReaderWriterPosix::Wait() {
  for (Provider* provider : providers_) {
    provider->PerformNetworkingOperations();
  }
}

void NetworkReaderWriterPosix::RunUntilStopped() {
  const bool was_running = is_running_.exchange(true);
  OSP_CHECK(!was_running);

  while (is_running_) {
    Wait();
  }
}

void NetworkReaderWriterPosix::RequestStopSoon() {
  is_running_.store(false);
}

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_NETWORK_READER_WRITER_H_
