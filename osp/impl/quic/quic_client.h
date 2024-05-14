// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_CLIENT_H_
#define OSP_IMPL_QUIC_QUIC_CLIENT_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "osp/impl/quic/quic_connection_factory_client.h"
#include "osp/impl/quic/quic_service_common.h"
#include "osp/public/endpoint_config.h"
#include "osp/public/protocol_connection_client.h"
#include "platform/api/task_runner.h"
#include "platform/api/time.h"
#include "platform/base/ip_address.h"
#include "util/alarm.h"

namespace openscreen::osp {

// This class is the default implementation of ProtocolConnectionClient for the
// library.  It manages connections to other endpoints as well as the lifetime
// of each incoming and outgoing stream.  It works in conjunction with a
// QuicConnectionFactoryClient and MessageDemuxer.
// QuicConnectionFactoryClient provides the actual ability to make a new QUIC
// connection with another endpoint.  Incoming data is given to the QuicClient
// by the underlying QUIC implementation (through QuicConnectionFactoryClient)
// and this is in turn handed to MessageDemuxer for routing CBOR messages.
//
// The two most significant methods of this class are Connect and
// CreateProtocolConnection.  Both will return a new QUIC stream to a given
// endpoint to which the caller can write but the former is allowed to be
// asynchronous.  If there isn't currently a connection to the specified
// endpoint, Connect will start a connection attempt and store the callback for
// when the connection completes.  CreateProtocolConnection simply returns
// nullptr if there's no existing connection.
class QuicClient final : public ProtocolConnectionClient,
                         public ServiceConnectionDelegate::ServiceDelegate {
 public:
  QuicClient(const EndpointConfig& config,
             MessageDemuxer& demuxer,
             std::unique_ptr<QuicConnectionFactoryClient> connection_factory,
             ProtocolConnectionServiceObserver& observer,
             ClockNowFunctionPtr now_function,
             TaskRunner& task_runner);
  ~QuicClient() override;

  // ProtocolConnectionClient overrides.
  bool Start() override;
  bool Stop() override;
  ConnectRequest Connect(const std::string& instance_id,
                         ConnectionRequestCallback* request) override;
  std::unique_ptr<ProtocolConnection> CreateProtocolConnection(
      uint64_t instance_number) override;

  // QuicProtocolConnection::Owner overrides.
  void OnConnectionDestroyed(QuicProtocolConnection* connection) override;

  // ServiceConnectionDelegate::ServiceDelegate overrides.
  uint64_t OnCryptoHandshakeComplete(ServiceConnectionDelegate* delegate,
                                     std::string connection_id) override;
  void OnIncomingStream(
      std::unique_ptr<QuicProtocolConnection> connection) override;
  void OnConnectionClosed(uint64_t instance_number,
                          std::string connection_id) override;
  void OnDataReceived(uint64_t instance_number,
                      uint64_t protocol_connection_id,
                      const ByteView& bytes) override;

 private:
  // FakeQuicBridge needs to access |instance_infos_| and struct InstanceInfo
  // for tests.
  friend class FakeQuicBridge;

  struct PendingConnectionData {
    explicit PendingConnectionData(ServiceConnectionData&& data);
    PendingConnectionData(PendingConnectionData&&) noexcept;
    ~PendingConnectionData();
    PendingConnectionData& operator=(PendingConnectionData&&) noexcept;

    ServiceConnectionData data;

    // Pairs of request IDs and the associated connection callback.
    std::vector<std::pair<uint64_t, ConnectionRequestCallback*>> callbacks;
  };

  // This struct holds necessary information of an instance used to build
  // connection.
  struct InstanceInfo {
    // Agent fingerprint.
    std::string fingerprint;

    // The network endpoints to create a new connection to the Open Screen
    // service. At least one of them is valid and use |v4_endpoint| first if it
    // is valid.
    IPEndpoint v4_endpoint;
    IPEndpoint v6_endpoint;
  };

  // ServiceListener::Observer overrides.
  void OnStarted() override;
  void OnStopped() override;
  void OnSuspended() override;
  void OnSearching() override;
  void OnReceiverAdded(const ServiceInfo& info) override;
  void OnReceiverChanged(const ServiceInfo& info) override;
  void OnReceiverRemoved(const ServiceInfo& info) override;
  void OnAllReceiversRemoved() override;
  void OnError(const Error& error) override;
  void OnMetrics(ServiceListener::Metrics) override;

  ConnectRequest CreatePendingConnection(const std::string& instance_id,
                                         ConnectionRequestCallback* request);
  uint64_t StartConnectionRequest(const std::string& instance_id,
                                  ConnectionRequestCallback* request);
  void CloseAllConnections();
  std::unique_ptr<QuicProtocolConnection> MakeProtocolConnection(
      QuicConnection* connection,
      ServiceConnectionDelegate* delegate,
      uint64_t instance_number);

  void CancelConnectRequest(uint64_t request_id) override;

  // Deletes dead QUIC connections then returns the time interval before this
  // method should be run again.
  void Cleanup();

  std::unique_ptr<QuicConnectionFactoryClient> connection_factory_;

  // IPEndpoints used by this client to build connection.
  //
  // NOTE: Only the first one is used currently. A better way is needed to
  // handle multiple IPEndpoints situations.
  std::vector<IPEndpoint> connection_endpoints_;

  // Maps an instance ID to a generated instance number. An instance is
  // identitied by instance ID before connection is built and is identitied by
  // instance number for simplicity after then. See OnCryptoHandshakeComplete
  // method.  This is used to insulate callers from post-handshake changes to a
  // connections actual peer instance.
  std::map<std::string, uint64_t> instance_map_;

  // Value that will be used for the next new instance in a Connect call.
  uint64_t next_instance_number_ = 0;

  // Value that will be used for the next new connection request.
  uint64_t next_request_id_ = 1;

  // Maps an instance ID to data about connections that haven't successfully
  // completed the QUIC handshake.
  std::map<std::string, PendingConnectionData> pending_connections_;

  // Maps an instance number to data about connections that have successfully
  // completed the QUIC handshake.
  std::map<uint64_t, ServiceConnectionData> connections_;

  // Connections (instance numbers) that need to be destroyed, but have to wait
  // for the next event loop due to the underlying QUIC implementation's way of
  // referencing them.
  std::vector<uint64_t> delete_connections_;

  // Maps an instance ID to necessary information of the instance used to build
  // connection.
  std::map<std::string, InstanceInfo> instance_infos_;

  Alarm cleanup_alarm_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_CLIENT_H_
