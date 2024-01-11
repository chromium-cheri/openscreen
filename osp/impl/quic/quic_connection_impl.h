// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_CONNECTION_IMPL_H_
#define OSP_IMPL_QUIC_QUIC_CONNECTION_IMPL_H_

#include <memory>
#include <string>
#include <utility>

#include "osp/impl/quic/open_screen_session_base.h"
#include "osp/impl/quic/quic_connection.h"
#include "platform/api/udp_socket.h"
#include "quiche/quic/core/quic_clock.h"
#include "quiche/quic/core/quic_connection.h"

namespace openscreen::osp {

class QuicConnectionFactoryImpl;

class QuicConnectionImpl final : public QuicConnection,
                                 public OpenScreenSessionBase::Visitor {
 public:
  QuicConnectionImpl(QuicConnectionFactoryImpl* parent_factory,
                     QuicConnection::Delegate* delegate,
                     const quic::QuicClock* clock);
  QuicConnectionImpl(const QuicConnectionImpl&) = delete;
  QuicConnectionImpl& operator=(const QuicConnectionImpl&) = delete;
  ~QuicConnectionImpl() override;

  // UdpSocket::Client overrides.
  void OnRead(UdpSocket* socket, ErrorOr<UdpPacket> packet) override;
  void OnError(UdpSocket* socket, Error error) override;
  void OnSendError(UdpSocket* socket, Error error) override;

  // QuicConnection overrides.
  QuicStream* MakeOutgoingStream(QuicStream::Delegate* delegate) override;
  void Close() override;

  // quic::QuicSession::Visitor overrides
  void OnConnectionClosed(quic::QuicConnectionId server_connection_id,
                          quic::QuicErrorCode error_code,
                          const std::string& error_details,
                          quic::ConnectionCloseSource source) override;
  void OnWriteBlocked(
      quic::QuicBlockedWriterInterface* /*blocked_writer*/) override {}
  void OnRstStreamReceived(const quic::QuicRstStreamFrame& /*frame*/) override {
  }
  void OnStopSendingReceived(
      const quic::QuicStopSendingFrame& /*frame*/) override {}
  bool TryAddNewConnectionId(
      const quic::QuicConnectionId& /*server_connection_id*/,
      const quic::QuicConnectionId& /*new_connection_id*/) override {
    return false;
  }
  void OnConnectionIdRetired(
      const quic::QuicConnectionId& /*server_connection_id*/) override {}
  void OnServerPreferredAddressAvailable(
      const quic::QuicSocketAddress& /*server_preferred_address*/) override {}

  // OpenScreenSessionBase::Visitor overrides
  void OnCryptoHandshakeComplete() override;
  void OnIncomingStream(QuicStream* QuicStream) override;
  Delegate* GetConnectionDelegate() override { return delegate_; }

  void set_session(std::unique_ptr<OpenScreenSessionBase> session) {
    session_ = std::move(session);
  }

 private:
  QuicConnectionFactoryImpl* const parent_factory_;
  const quic::QuicClock* const clock_;  // Not owned.
  std::unique_ptr<OpenScreenSessionBase> session_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_CONNECTION_IMPL_H_
