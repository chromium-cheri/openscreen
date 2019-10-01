// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "platform/impl/network_reader_writer.h"

#include <algorithm>

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {

NetworkReaderWriter::NetworkReaderWriter(std::vector<Provider*> providers)
    : providers_(providers) {}

void NetworkReaderWriter::Wait() {
  for (Provider* provider : providers_) {
    provider->PerformNetworkingOperations();
  }
}

void NetworkReaderWriter::RunUntilStopped() {
  const bool was_running = is_running_.exchange(true);
  OSP_CHECK(!was_running);

  while (is_running_) {
    Wait();
  }
}

void NetworkReaderWriter::RequestStopSoon() {
  is_running_.store(false);
}

}  // namespace platform
}  // namespace openscreen
