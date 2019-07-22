// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/testing/quic_test_support.h"

#include <memory>

#include "osp/impl/quic/quic_client.h"
#include "osp/impl/quic/quic_server.h"
#include "osp/public/network_service_manager.h"

namespace openscreen {

FakeQuicBridge::FakeQuicBridge(platform::FakeNetworkRunner* network_runner,
                               platform::ClockNowFunctionPtr now_function)
    : network_runner_(network_runner) {
  std::cout << "FakeQuicBridge 1\n";
  std::cout.flush();
  fake_bridge =
      std::make_unique<FakeQuicConnectionFactoryBridge>(kControllerEndpoint);

  std::cout << "FakeQuicBridge 2\n";
  std::cout.flush();
  controller_demuxer = std::make_unique<MessageDemuxer>(
      now_function, MessageDemuxer::kDefaultBufferLimit);
  receiver_demuxer = std::make_unique<MessageDemuxer>(
      now_function, MessageDemuxer::kDefaultBufferLimit);

  std::cout << "FakeQuicBridge 3\n";
  std::cout.flush();
  auto fake_client_factory =
      std::make_unique<FakeClientQuicConnectionFactory>(fake_bridge.get());

  std::cout << "FakeQuicBridge 3.1\n";
  std::cout.flush();
  client_socket_ = std::make_unique<platform::MockUdpSocket>();

  std::cout << "FakeQuicBridge 3.2\n";
  std::cout.flush();
  network_runner_->ReadRepeatedly(client_socket_.get(),
                                  fake_client_factory.get());

  std::cout << "FakeQuicBridge 3.3\n";
  std::cout.flush();
  quic_client = std::make_unique<QuicClient>(controller_demuxer.get(),
                                             std::move(fake_client_factory),
                                             &mock_client_observer);

  std::cout << "FakeQuicBridge 4\n";
  std::cout.flush();
  auto fake_server_factory =
      std::make_unique<FakeServerQuicConnectionFactory>(fake_bridge.get());
  server_socket_ = std::make_unique<platform::MockUdpSocket>();
  network_runner_->ReadRepeatedly(server_socket_.get(),
                                  fake_server_factory.get());

  std::cout << "FakeQuicBridge 5\n";
  std::cout.flush();
  ServerConfig config;
  config.connection_endpoints.push_back(kReceiverEndpoint);
  quic_server = std::make_unique<QuicServer>(config, receiver_demuxer.get(),
                                             std::move(fake_server_factory),
                                             &mock_server_observer);

  std::cout << "FakeQuicBridge 6\n";
  std::cout.flush();
  quic_client->Start();
  quic_server->Start();
}

FakeQuicBridge::~FakeQuicBridge() = default;

void FakeQuicBridge::PostClientPacket() {
  auto packet = std::make_unique<platform::UdpReadCallback::Packet>();
  packet->socket = client_socket_.get();
  network_runner_->PostNewPacket(std::move(packet));
}

void FakeQuicBridge::PostServerPacket() {
  auto packet = std::make_unique<platform::UdpReadCallback::Packet>();
  packet->socket = server_socket_.get();
  network_runner_->PostNewPacket(std::move(packet));
}

void FakeQuicBridge::PostPacketsUntilIdle() {
  bool client_idle = fake_bridge->client_idle();
  bool server_idle = fake_bridge->server_idle();
  if (!client_idle || !server_idle) {
    PostClientPacket();
    PostServerPacket();
    network_runner_->PostTask(
        std::bind(&FakeQuicBridge::PostPacketsUntilIdle, this));
  }
}

void FakeQuicBridge::RunTasksUntilIdle() {
  PostClientPacket();
  PostServerPacket();
  network_runner_->PostTask(
      std::bind(&FakeQuicBridge::PostPacketsUntilIdle, this));
  network_runner_->RunTasksUntilIdle();
}

}  // namespace openscreen
