// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/protocol_connection_server_factory.h"

#include "api/impl/quic/quic_connection_factory_impl.h"
#include "api/impl/quic/quic_server.h"
#include "third_party/abseil/src/absl/memory/memory.h"

namespace openscreen {

// static
std::unique_ptr<ProtocolConnectionServer>
ProtocolConnectionServerFactory::Create(
    const ServerConfig& config,
    MessageDemuxer* demuxer,
    ProtocolConnectionServer::Observer* observer) {
  return absl::make_unique<QuicServer>(
      config, demuxer, absl::make_unique<QuicConnectionFactoryImpl>(),
      observer);
}

}  // namespace openscreen
