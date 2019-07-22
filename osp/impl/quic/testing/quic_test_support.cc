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
  fake_bridge =
      std::make_unique<FakeQuicConnectionFactoryBridge>(kControllerEndpoint);

  controller_demuxer = std::make_unique<MessageDemuxer>(
      now_function, MessageDemuxer::kDefaultBufferLimit);
  receiver_demuxer = std::make_unique<MessageDemuxer>(
      now_function, MessageDemuxer::kDefaultBufferLimit);

  auto fake_client_factory =
      std::make_unique<FakeClientQuicConnectionFactory>(fake_bridge.get());
  client_socket_ = std::make_unique<platform::MockUdpSocket>();
  network_runner_->ReadRepeatedly(client_socket_.get(),
                                  fake_client_factory.get());
  quic_client = std::make_unique<QuicClient>(controller_demuxer.get(),
                                             std::move(fake_client_factory),
                                             &mock_client_observer);

  auto fake_server_factory =
      std::make_unique<FakeServerQuicConnectionFactory>(fake_bridge.get());
  server_socket_ = std::make_unique<platform::MockUdpSocket>();
  network_runner_->ReadRepeatedly(server_socket_.get(),
                                  fake_server_factory.get());
  ServerConfig config;
  config.connection_endpoints.push_back(kReceiverEndpoint);
  quic_server = std::make_unique<QuicServer>(config, receiver_demuxer.get(),
                                             std::move(fake_server_factory),
                                             &mock_server_observer);

  quic_client->Start();
  quic_server->Start();
}

FakeQuicBridge::~FakeQuicBridge() = default;

void FakeQuicBridge::PostClientPacket() {
  std::cout << "PostClientPacket\n";
  std::cout.flush();
  auto packet = std::make_unique<platform::UdpReadCallback::Packet>();
  packet->socket = client_socket_.get();
  network_runner_->PostNewPacket(std::move(packet));
}

void FakeQuicBridge::PostServerPacket() {
  std::cout << "PostServerPacket\n";
  std::cout.flush();
  auto packet = std::make_unique<platform::UdpReadCallback::Packet>();
  packet->socket = server_socket_.get();
  network_runner_->PostNewPacket(std::move(packet));
}

void FakeQuicBridge::PostPacketsUntilIdle() {
  std::cout << "PostPacketsUntilIdle\n";
  std::cout.flush();
  bool client_idle = fake_bridge->client_idle();
  std::cout << "Result: client '" << !!client_idle << "'\n";
  bool server_idle = fake_bridge->server_idle();
  std::cout << "Result: server '" << !!server_idle << "'\n";
  std::cout.flush();
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
