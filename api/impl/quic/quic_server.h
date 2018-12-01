// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_QUIC_QUIC_SERVER_H_
#define API_IMPL_QUIC_QUIC_SERVER_H_

#include <cstdint>
#include <map>
#include <memory>

#include "api/impl/quic/quic_connection_factory.h"
#include "api/impl/quic/quic_service_common.h"
#include "api/public/protocol_connection_server.h"
#include "base/ip_address.h"

namespace openscreen {

// This class is the default implementation of ProtocolConnectionServer for the
// library.  It manages connections to other endpoints as well as the lifetime
// of each incoming and outgoing stream.  It works in conjunction with a
// QuicConnectionFactory implementation and MessageDemuxer.
// QuicConnectionFactory provides the ability to make a new QUIC
// connection from packets received on its server sockets.  Incoming data is
// given to the QuicServer by the underlying QUIC implementation (through
// QuicConnectionFactory) and this is in turn handed to MessageDemuxer for
// routing CBOR messages.
class QuicServer final : public ProtocolConnectionServer,
                         public QuicConnectionFactory::ServerDelegate,
                         public ServiceConnectionDelegate::ServiceDelegate {
 public:
  QuicServer(const ServerConfig& config,
             MessageDemuxer* demuxer,
             std::unique_ptr<QuicConnectionFactory>&& connection_factory,
             ProtocolConnectionServer::Observer* observer);
  ~QuicServer() override;

  // ProtocolConnectionServer overrides.
  bool Start() override;
  bool Stop() override;
  bool Suspend() override;
  bool Resume() override;
  void RunTasks() override;

  // QuicProtocolConnection::Owner overrides.
  void OnConnectionDestroyed(QuicProtocolConnection* connection) override;

  // ServiceConnectionDelegate::ServiceDelegate overrides.
  uint64_t OnCryptoHandshakeComplete(ServiceConnectionDelegate* delegate,
                                     uint64_t connection_id) override;
  void OnIncomingStream(
      std::unique_ptr<QuicProtocolConnection>&& connection) override;
  void OnConnectionClosed(uint64_t endpoint_id,
                          uint64_t connection_id) override;
  void OnDataReceived(uint64_t endpoint_id,
                      uint64_t connection_id,
                      const uint8_t* data,
                      size_t data_size) override;

 private:
  void CloseAllConnections();

  // QuicConnectionFactory::ServerDelegate overrides.
  QuicConnection::Delegate* NextConnectionDelegate(
      const IPEndpoint& source) override;
  void OnIncomingConnection(
      std::unique_ptr<QuicConnection>&& connection) override;

  std::vector<IPEndpoint> connection_endpoints_;
  std::unique_ptr<QuicConnectionFactory> connection_factory_;

  std::unique_ptr<ServiceConnectionDelegate> pending_connection_delegate_;
  std::map<IPEndpoint, uint64_t, IPEndpointComparator> endpoint_map_;
  uint64_t next_endpoint_id_ = 0;

  std::map<IPEndpoint, ServiceConnectionData, IPEndpointComparator>
      pending_connections_;
  std::map<uint64_t, ServiceConnectionData> connections_;
  std::vector<ServiceConnectionData> delete_connections_;
};

}  // namespace openscreen

#endif  // API_IMPL_QUIC_QUIC_SERVER_H_
