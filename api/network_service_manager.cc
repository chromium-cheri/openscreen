// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/network_control_service.h"

#include "api/mdns_screen_listener.h"
#include "api/mdns_screen_publisher.h"
#include "api/screen_connection_client.h"
#include "api/screen_connection_server.h"

namespace {

NetworkControlService* g_instance = nullptr;

} //  namespace

namespace openscreen {

// static
NetworkControlService* NetworkControlService::Create(
    const MdnsScreenListener* mdns_listener,
    const MdnsScreenListener* mdns_publisher,
    const ScreenConnectionClient* connection_client,
    const ScreenConnectionServer* connection_server) {
  // TODO(mfoltz): Convert to assertion failure
  if (g_instance) return nullptr;
  g_instance = new NetworkController(mdns_listener, mdns_publisher, connection_client, connection_server);
  return g_instance;
}

// static
NetworkControlService* NetworkControlService::Get() {
  // TODO(mfoltz): Convert to assertion failure
  if (!g_instance) return nullptr;
  return g_instance;
}

void NetworkControlService::Dispose() {
  NetworkControlService* instance = Get();
  // TODO(mfoltz): Convert to assertion failure
  if (!g_instance) return;
  delete g_instance;
  g_instance = nullptr;
}

MdnsScreenListener* NetworkControlService::GetMdnsScreenListener() {
  return mdns_listener_;
}

MdnsScreenPublisher* NetworkControlService::GetMdnsScreenPublisher() {
  return mdns_publisher_;
}

ScreenConnectionClient* NetworkControlService::GetScreenConnectionClient() {
  return connection_client_;
}

ScreenConnectionServer* NetworkControlService::GetScreenConnectionServer() {
  return connection_server_;
}

NetworkControlService(const MdnsScreenListener* mdns_listener,
                      const MdnsScreenListener* mdns_publisher,
                      const ScreenConnectionClient* connection_client,
                      const ScreenConnectionServer* connection_server)
  : mdns_listener_(mdns_listener),
    mdns_publisher_(mdns_publisher),
    connection_client_(connection_client),
    connection_server_(connection_server) { }

~NetworkControlService::NetworkControlService() {
  if (mdns_listener_) delete mdns_listener_;
  if (mdns_publisher_) delete mdns_publisher_;
  if (connection_client_) delete connection_client_;
  if (connection_server_) delete connection_server_;
}

}  // namespace openscreen

#endif  // API_NETWORK_CONTROLLER_H_
