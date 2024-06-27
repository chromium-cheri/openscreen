// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_SERVER_H_
#define OSP_IMPL_QUIC_QUIC_SERVER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "osp/impl/quic/quic_connection_factory_server.h"
#include "osp/impl/quic/quic_service_base.h"
#include "osp/public/instance_request_ids.h"
#include "osp/public/protocol_connection_server.h"

namespace openscreen::osp {

// This class is the default implementation of ProtocolConnectionServer for the
// library.  It manages connections to other endpoints as well as the lifetime
// of each incoming and outgoing stream.  It works in conjunction with a
// QuicConnectionFactoryServer and MessageDemuxer.
// QuicConnectionFactoryServer provides the ability to make a new QUIC
// connection from packets received on its server sockets.  Incoming data is
// given to the QuicServer by the underlying QUIC implementation (through
// QuicConnectionFactoryServer) and this is in turn handed to MessageDemuxer for
// routing CBOR messages.
class QuicServer final : public ProtocolConnectionServer,
                         public QuicConnectionFactoryServer::ServerDelegate,
                         public QuicServiceBase {
 public:
  QuicServer(const EndpointConfig& config,
             MessageDemuxer& demuxer,
             std::unique_ptr<QuicConnectionFactoryServer> connection_factory,
             ProtocolConnectionServiceObserver& observer,
             ClockNowFunctionPtr now_function,
             TaskRunner& task_runner);
  QuicServer(const QuicServer&) = delete;
  QuicServer& operator=(const QuicServer&) = delete;
  QuicServer(QuicServer&&) noexcept = delete;
  QuicServer& operator=(QuicServer&&) noexcept = delete;
  ~QuicServer() override;

  // ProtocolConnectionServer overrides.
  bool Start() override;
  bool Stop() override;
  bool Suspend() override;
  bool Resume() override;
  State GetState() override;
  MessageDemuxer* GetMessageDemuxer() override;
  InstanceRequestIds* GetInstanceRequestIds() override;
  std::unique_ptr<ProtocolConnection> CreateProtocolConnection(
      uint64_t instance_id) override;
  std::string GetFingerprint() override;

  // QuicServiceBase overrides.
  uint64_t OnCryptoHandshakeComplete(ServiceConnectionDelegate* delegate,
                                     std::string connection_id) override;
  void OnConnectionClosed(uint64_t instance_id,
                          std::string connection_id) override;

 private:
  // QuicServiceBase overrides.
  void CloseAllConnections() override;

  // QuicConnectionFactoryServer::ServerDelegate overrides.
  QuicConnection::Delegate* NextConnectionDelegate(
      const IPEndpoint& source) override;
  void OnIncomingConnection(
      std::unique_ptr<QuicConnection> connection) override;

  InstanceRequestIds instance_request_ids_;
  std::unique_ptr<QuicConnectionFactoryServer> connection_factory_;
  std::unique_ptr<ServiceConnectionDelegate> pending_connection_delegate_;

  // Maps an instance name to data about connections that haven't successfully
  // completed the QUIC handshake.
  std::map<std::string, ServiceConnectionData> pending_connections_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_SERVER_H_
