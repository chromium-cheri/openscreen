// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_QUIC_QUIC_CLIENT_H_
#define API_IMPL_QUIC_QUIC_CLIENT_H_

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "api/impl/quic/quic_connection_factory.h"
#include "api/impl/quic/quic_service_common.h"
#include "api/public/protocol_connection_client.h"
#include "base/ip_address.h"

namespace openscreen {

// This class is the default implementation of ProtocolConnectionClient for the
// library.  It manages connections to other endpoints as well as the lifetime
// of each incoming and outgoing stream.  It works in conjunction with a
// QuicConnectionFactory implementation and MessageDemuxer.
// QuicConnectionFactory provides the actual ability to make a new QUIC
// connection with another endpoint.  Incoming data is given to the QuicClient
// by the underlying QUIC implementation (through QuicConnectionFactory) and
// this is in turn handed to MessageDemuxer for routing CBOR messages.
//
// Thw two most significant methods of this class are OpenConnection and
// OpenConnectionNow.  Both will return a new QUIC stream to a given endpoint to
// which the caller can write but the former is allowed to be asynchronous.  If
// there isn't currently a connection to the specified endpoint, OpenConnection
// will start a connection attempt and store the callback for when the
// connection completes.  OpenConnectionNow simply returns nullptr if there's no
// existing connection.
class QuicClient final : public ProtocolConnectionClient,
                         public ServiceConnectionDelegate::ServiceDelegate {
 public:
  QuicClient(MessageDemuxer* demuxer,
             std::unique_ptr<QuicConnectionFactory>&& connection_factory,
             ProtocolConnectionServiceObserver* observer);
  ~QuicClient() override;

  bool Start() override;
  bool Stop() override;
  void RunTasks() override;
  ConnectRequest OpenConnection(const IPEndpoint& endpoint,
                                ConnectionRequestCallback* request) override;
  std::unique_ptr<ProtocolConnection> OpenConnectionNow(
      const IPEndpoint& endpoint) override;

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
  struct PendingConnectionData {
    explicit PendingConnectionData(ServiceConnectionData&& data);
    PendingConnectionData(PendingConnectionData&&);
    ~PendingConnectionData();
    PendingConnectionData& operator=(PendingConnectionData&&);

    ServiceConnectionData data;
    std::vector<std::pair<uint64_t, ConnectionRequestCallback*>> callbacks;
  };

  uint64_t StartConnectionRequest(const IPEndpoint& endpoint,
                                  ConnectionRequestCallback* request);
  void CloseAllConnections();

  void CancelConnectRequest(uint64_t request_id) override;

  std::unique_ptr<QuicConnectionFactory> connection_factory_;

  std::map<IPEndpoint, uint64_t, IPEndpointComparator> endpoint_map_;
  uint64_t next_endpoint_id_ = 1;

  std::map<uint64_t, std::pair<IPEndpoint, ConnectionRequestCallback*>>
      request_map_;
  uint64_t next_request_id_ = 1;

  std::map<IPEndpoint, PendingConnectionData, IPEndpointComparator>
      pending_connections_;
  std::map<uint64_t, ServiceConnectionData> connections_;
  std::vector<ServiceConnectionData> delete_connections_;
};

}  // namespace openscreen

#endif  // API_IMPL_QUIC_QUIC_CLIENT_H_
