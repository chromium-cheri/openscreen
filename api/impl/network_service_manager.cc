// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/network_service_manager.h"

#include "api/impl/internal_services.h"
#include "api/impl/message_demuxer.h"
#include "api/impl/quic/quic_connection_factory_impl.h"
#include "api/impl/quic/quic_service.h"

namespace {

openscreen::NetworkServiceManager* g_network_service_manager_instance = nullptr;

}  //  namespace

namespace openscreen {

// static
NetworkServiceManager* NetworkServiceManager::Create(
    std::unique_ptr<ScreenListener> mdns_listener,
    std::unique_ptr<ScreenPublisher> mdns_publisher,
    std::unique_ptr<ProtocolConnectionClient> connection_client,
    std::unique_ptr<ProtocolConnectionServer> connection_server) {
  // TODO(mfoltz): Convert to assertion failure
  if (g_network_service_manager_instance)
    return nullptr;
  g_network_service_manager_instance = new NetworkServiceManager(
      std::move(mdns_listener), std::move(mdns_publisher),
      std::move(connection_client), std::move(connection_server));
  return g_network_service_manager_instance;
}

// static
NetworkServiceManager* NetworkServiceManager::Get() {
  // TODO(mfoltz): Convert to assertion failure
  if (!g_network_service_manager_instance)
    return nullptr;
  return g_network_service_manager_instance;
}

// static
void NetworkServiceManager::Dispose() {
  QuicService::Get()->CloseAllConnections();
  // TODO(mfoltz): Convert to assertion failure
  if (!g_network_service_manager_instance)
    return;
  delete g_network_service_manager_instance;
  g_network_service_manager_instance = nullptr;
}

void NetworkServiceManager::RunEventLoopOnce() {
  InternalServices::RunEventLoopOnce();
  QuicConnectionFactory::Get()->RunTasks();
}

void NetworkServiceManager::InitSingletonServices() {
  if (!QuicConnectionFactory::Get()) {
    DCHECK(!quic_connection_factory_);
    quic_connection_factory_.reset(new QuicConnectionFactoryImpl);
    QuicConnectionFactory::Set(quic_connection_factory_.get());
  }
  if (!QuicService::Get()) {
    DCHECK(!quic_service_);
    quic_service_.reset(new QuicService);
    QuicService::Set(quic_service_.get());
  }
  if (!MessageDemuxer::Get()) {
    DCHECK(!message_demuxer_);
    message_demuxer_.reset(new MessageDemuxer);
    MessageDemuxer::Set(message_demuxer_.get());
  }
}

ScreenListener* NetworkServiceManager::GetMdnsScreenListener() {
  return mdns_listener_.get();
}

ScreenPublisher* NetworkServiceManager::GetMdnsScreenPublisher() {
  return mdns_publisher_.get();
}

ProtocolConnectionClient* NetworkServiceManager::GetProtocolConnectionClient() {
  return connection_client_.get();
}

ProtocolConnectionServer* NetworkServiceManager::GetProtocolConnectionServer() {
  return connection_server_.get();
}

NetworkServiceManager::NetworkServiceManager(
    std::unique_ptr<ScreenListener> mdns_listener,
    std::unique_ptr<ScreenPublisher> mdns_publisher,
    std::unique_ptr<ProtocolConnectionClient> connection_client,
    std::unique_ptr<ProtocolConnectionServer> connection_server)
    : mdns_listener_(std::move(mdns_listener)),
      mdns_publisher_(std::move(mdns_publisher)),
      connection_client_(std::move(connection_client)),
      connection_server_(std::move(connection_server)) {}

NetworkServiceManager::~NetworkServiceManager() = default;

}  // namespace openscreen
