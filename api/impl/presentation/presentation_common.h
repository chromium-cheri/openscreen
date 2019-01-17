// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_PRESENTATION_PRESENTATION_COMMON_H_
#define API_IMPL_PRESENTATION_PRESENTATION_COMMON_H_
#include <algorithm>
#include <memory>

#include "api/public/message_demuxer.h"
#include "api/public/network_service_manager.h"
#include "api/public/protocol_connection_server.h"
#include "msgs/osp_messages.h"
#include "platform/api/logging.h"
#include "platform/api/time.h"

namespace openscreen {
namespace presentation {
template <typename T>
void WriteMessage(const T& message,
                  bool (*encoder)(const T&, msgs::CborEncodeBuffer*),
                  ProtocolConnection* connection) {
  msgs::CborEncodeBuffer buffer;

  if (!encoder(message, &buffer)) {
    OSP_LOG_WARN << "failed to properly encode presentation message";
    return;
  }

  connection->Write(buffer.data(), buffer.size());
}

std::unique_ptr<ProtocolConnection> GetEndpointConnection(uint64_t endpoint_id);
}  // namespace presentation
}  // namespace openscreen

#endif