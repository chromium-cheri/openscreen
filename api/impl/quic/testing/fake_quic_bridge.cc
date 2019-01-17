// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/quic/testing/fake_quic_bridge.h"

#include "api/impl/quic/quic_client.h"
#include "api/impl/quic/quic_server.h"
#include "api/public/network_service_manager.h"
#include "base/make_unique.h"

namespace openscreen {

FakeQuicBridge::FakeQuicBridge() {
  fake_bridge =
      MakeUnique<FakeQuicConnectionFactoryBridge>(controller_endpoint);
  platform::TimeDelta now = platform::TimeDelta::FromMilliseconds(1298424);
  auto fake_clock = MakeUnique<FakeClock>(now);
  controller_fake_clock = fake_clock.get();
  controller_demuxer = MakeUnique<MessageDemuxer>(
      MessageDemuxer::kDefaultBufferLimit, std::move(fake_clock));

  fake_clock = MakeUnique<FakeClock>(now);
  receiver_fake_clock = fake_clock.get();
  receiver_demuxer = MakeUnique<MessageDemuxer>(
      MessageDemuxer::kDefaultBufferLimit, std::move(fake_clock));

  auto fake_client_factory =
      MakeUnique<FakeClientQuicConnectionFactory>(fake_bridge.get());
  auto quic_client = MakeUnique<QuicClient>(controller_demuxer.get(),
                                            std::move(fake_client_factory),
                                            &mock_client_observer);
  auto fake_server_factory =
      MakeUnique<FakeServerQuicConnectionFactory>(fake_bridge.get());
  ServerConfig config;
  config.connection_endpoints.push_back(receiver_endpoint);
  auto quic_server = MakeUnique<QuicServer>(config, receiver_demuxer.get(),
                                            std::move(fake_server_factory),
                                            &mock_server_observer);
  quic_client->Start();
  quic_server->Start();
  NetworkServiceManager::Get()->Create(nullptr, nullptr, std::move(quic_client),
                                       std::move(quic_server));
}

FakeQuicBridge::~FakeQuicBridge() {
  NetworkServiceManager::Get()->Dispose();
}

void FakeQuicBridge::RunTasksUntilIdle() {
  do {
    NetworkServiceManager::Get()->GetProtocolConnectionClient()->RunTasks();
    NetworkServiceManager::Get()->GetProtocolConnectionServer()->RunTasks();
  } while (!fake_bridge->idle());
}

}  // namespace openscreen
