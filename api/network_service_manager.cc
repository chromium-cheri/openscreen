// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/network_service_manager.h"

#include "api/mdns_screen_listener.h"
#include "api/mdns_screen_publisher.h"
#include "api/screen_connection_client.h"
#include "api/screen_connection_server.h"

namespace {

openscreen::NetworkServiceManager* g_network_service_manager_instance = nullptr;

} //  namespace

namespace openscreen {

// static
NetworkServiceManager* NetworkServiceManager::Create(
    MdnsScreenListener* mdns_listener,
    MdnsScreenPublisher* mdns_publisher,
    ScreenConnectionClient* connection_client,
    ScreenConnectionServer* connection_server) {
  // TODO(mfoltz): Convert to assertion failure
  if (g_network_service_manager_instance) return nullptr;
  g_network_service_manager_instance = new NetworkServiceManager(mdns_listener, mdns_publisher, connection_client, connection_server);
  return g_network_service_manager_instance;
}

// static
NetworkServiceManager* NetworkServiceManager::Get() {
  // TODO(mfoltz): Convert to assertion failure
  if (!g_network_service_manager_instance) return nullptr;
  return g_network_service_manager_instance;
}

// static
void NetworkServiceManager::Dispose() {
  // TODO(mfoltz): Convert to assertion failure
  if (!g_network_service_manager_instance) return;
  delete g_network_service_manager_instance;
  g_network_service_manager_instance = nullptr;
}

MdnsScreenListener* NetworkServiceManager::GetMdnsScreenListener() {
  return mdns_listener_;
}

MdnsScreenPublisher* NetworkServiceManager::GetMdnsScreenPublisher() {
  return mdns_publisher_;
}

ScreenConnectionClient* NetworkServiceManager::GetScreenConnectionClient() {
  return connection_client_;
}

ScreenConnectionServer* NetworkServiceManager::GetScreenConnectionServer() {
  return connection_server_;
}

NetworkServiceManager::NetworkServiceManager(
    MdnsScreenListener* mdns_listener,
    MdnsScreenPublisher* mdns_publisher,
    ScreenConnectionClient* connection_client,
    ScreenConnectionServer* connection_server)
  : mdns_listener_(mdns_listener),
    mdns_publisher_(mdns_publisher),
    connection_client_(connection_client),
    connection_server_(connection_server) { }

NetworkServiceManager::~NetworkServiceManager() {
  if (mdns_listener_) delete mdns_listener_;
  if (mdns_publisher_) delete mdns_publisher_;
  if (connection_client_) delete connection_client_;
  if (connection_server_) delete connection_server_;
}

}  // namespace openscreen
