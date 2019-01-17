// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/presentation/presentation_common.h"

namespace openscreen {
namespace presentation {
std::unique_ptr<ProtocolConnection> GetProtocolConnection(
    uint64_t endpoint_id) {
  return NetworkServiceManager::Get()
      ->GetProtocolConnectionServer()
      ->CreateProtocolConnection(endpoint_id);
}

MessageDemuxer* GetDemuxer() {
  return NetworkServiceManager::Get()
      ->GetProtocolConnectionServer()
      ->message_demuxer();
}
}  // namespace presentation

}  // namespace openscreen