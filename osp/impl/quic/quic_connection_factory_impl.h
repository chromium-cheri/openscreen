// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_CONNECTION_FACTORY_IMPL_H_
#define OSP_IMPL_QUIC_QUIC_CONNECTION_FACTORY_IMPL_H_

#include <map>
#include <memory>
#include <vector>

#include "osp/impl/quic/quic_connection_factory.h"
#include "platform/api/udp_socket.h"
#include "platform/base/ip_address.h"
#include "quiche/quic/core/crypto/quic_crypto_client_config.h"
#include "quiche/quic/core/crypto/quic_crypto_server_config.h"
#include "quiche/quic/core/deterministic_connection_id_generator.h"
#include "quiche/quic/core/quic_alarm_factory.h"
#include "quiche/quic/core/quic_connection.h"
#include "quiche/quic/core/quic_versions.h"
#include "util/osp_logging.h"

namespace openscreen::osp {

class QuicTaskRunner;

class QuicConnectionDebugVisitorimpl : public quic::QuicConnectionDebugVisitor {
 public:
  void OnPacketSent(quic::QuicPacketNumber /*packet_number*/,
                    quic::QuicPacketLength /*packet_length*/,
                    bool /*has_crypto_handshake*/,
                    quic::TransmissionType /*transmission_type*/,
                    quic::EncryptionLevel /*encryption_level*/,
                    const quic::QuicFrames& /*retransmittable_frames*/,
                    const quic::QuicFrames& /*nonretransmittable_frames*/,
                    quic::QuicTime /*sent_time*/,
                    uint32_t /*batch_id*/) override {
    OSP_LOG_INFO << "QuicConnectionDebugVisitor::OnPacketSent";
  }

  void OnPacketReceived(const quic::QuicSocketAddress& /*self_address*/,
                        const quic::QuicSocketAddress& /*peer_address*/,
                        const quic::QuicEncryptedPacket& /*packet*/) override {
    OSP_LOG_INFO << "QuicConnectionDebugVisitor::OnPacketReceived";
  }

  void OnIncorrectConnectionId(
      quic::QuicConnectionId /*connection_id*/) override {
    OSP_LOG_INFO << "QuicConnectionDebugVisitor::OnIncorrectConnectionId";
  }
};

class QuicConnectionFactoryImpl final : public QuicConnectionFactory {
 public:
  explicit QuicConnectionFactoryImpl(TaskRunner& task_runner);
  ~QuicConnectionFactoryImpl() override;

  // UdpSocket::Client overrides.
  void OnBound(UdpSocket* socket) override;
  void OnError(UdpSocket* socket, Error error) override;
  void OnSendError(UdpSocket* socket, Error error) override;
  void OnRead(UdpSocket* socket, ErrorOr<UdpPacket> packet) override;

  // QuicConnectionFactory overrides.
  void SetServerDelegate(ServerDelegate* delegate,
                         const std::vector<IPEndpoint>& endpoints) override;
  std::unique_ptr<QuicConnection> Connect(
      const IPEndpoint& local_endpoint,
      const IPEndpoint& remote_endpoint,
      QuicConnection::Delegate* connection_delegate) override;

  void OnConnectionClosed(QuicConnection* connection);

 private:
  std::unique_ptr<quic::QuicConnectionHelperInterface> helper_;
  std::unique_ptr<quic::QuicAlarmFactory> alarm_factory_;
  quic::ParsedQuicVersionVector supported_versions_;
  quic::DeterministicConnectionIdGenerator connection_id_generator_{
      quic::kQuicDefaultConnectionIdLength};
  ServerDelegate* server_delegate_ = nullptr;

  std::vector<std::unique_ptr<UdpSocket>> sockets_;

  struct OpenConnection {
    QuicConnection* connection;
    UdpSocket* socket;  // References one of the owned |sockets_|.
  };
  std::map<IPEndpoint, OpenConnection> connections_;

  // NOTE: Must be provided in constructor and stored as an instance variable
  // rather than using the static accessor method to allow for UTs to mock this
  // layer.
  TaskRunner& task_runner_;

  QuicConnectionDebugVisitorimpl debug_visitor_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_CONNECTION_FACTORY_IMPL_H_
