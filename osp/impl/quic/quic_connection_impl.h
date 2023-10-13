// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_CONNECTION_IMPL_H_
#define OSP_IMPL_QUIC_QUIC_CONNECTION_IMPL_H_

#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "osp/impl/quic/open_screen_session_base.h"
#include "osp/impl/quic/quic_connection.h"
#include "platform/api/udp_socket.h"
#include "platform/base/ip_address.h"
#include "quiche/quic/core/quic_clock.h"
#include "quiche/quic/core/quic_connection.h"
#include "quiche/quic/core/quic_packet_writer.h"
#include "quiche/quic/core/quic_stream.h"

namespace openscreen::osp {

class QuicConnectionFactoryImpl;

class PacketWriterImpl final : public quic::QuicPacketWriter {
 public:
  PacketWriterImpl(UdpSocket* socket, const IPEndpoint& destination);
  PacketWriterImpl(const PacketWriterImpl&) = delete;
  PacketWriterImpl& operator=(const PacketWriterImpl&) = delete;
  ~PacketWriterImpl() override;

  // quic::QuicPacketWriter overrides.
  quic::WriteResult WritePacket(
      const char* buffer,
      size_t buffer_length,
      const quic::QuicIpAddress& self_address,
      const quic::QuicSocketAddress& peer_address,
      quic::PerPacketOptions* options,
      const quic::QuicPacketWriterParams& params) override;
  bool IsWriteBlocked() const override;
  void SetWritable() override;
  absl::optional<int> MessageTooBigErrorCode() const override;
  quic::QuicByteCount GetMaxPacketSize(
      const quic::QuicSocketAddress& peer_address) const override;
  bool SupportsReleaseTime() const override;
  bool IsBatchMode() const override;
  bool SupportsEcn() const override;
  quic::QuicPacketBuffer GetNextWriteLocation(
      const quic::QuicIpAddress& self_address,
      const quic::QuicSocketAddress& peer_address) override;
  quic::WriteResult Flush() override;

 private:
  UdpSocket* socket_;
  IPEndpoint destination_;
};

class QuicStreamImpl final : public QuicStream, public quic::QuicStream {
 public:
  QuicStreamImpl(Delegate* delegate,
                 quic::QuicStreamId id,
                 quic::QuicSession* session,
                 quic::StreamType type);
  ~QuicStreamImpl() override;

  // QuicStream overrides.
  uint64_t GetStreamId() override;
  void Write(const uint8_t* data, size_t size) override;
  void CloseWriteEnd() override;

  // quic::QuicStream overrides.
  void OnDataAvailable() override;
  void OnClose() override;
};

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
