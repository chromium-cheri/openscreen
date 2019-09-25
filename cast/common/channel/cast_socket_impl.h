// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CHANNEL_CAST_SOCKET_IMPL_H_
#define CAST_COMMON_CHANNEL_CAST_SOCKET_IMPL_H_

#include "cast/common/channel/cast_socket.h"
#include "platform/api/tls_connection.h"

namespace cast {
namespace channel {

// Implements CastSocket for sending and receiving Cast V2 messages over a TLS
// connection provided by the platform layer.
class CastSocketImpl final
    : public CastSocket,
      public openscreen::platform::TlsConnection::Client {
 public:
  using TlsConnection = openscreen::platform::TlsConnection;

  CastSocketImpl(std::unique_ptr<TlsConnection> connection,
                 CastSocket::Delegate* delegate,
                 uint32_t socket_id);
  ~CastSocketImpl() override;

  // CastSocket overrides.
  // Sends |message| immediately unless the underlying TLS connection is
  // write-blocked, in which case |message| will be queued.  No error is
  // returned for both queueing and successful sending.
  Error SendMessage(const CastMessage& message) override;

  // TlsConnection::Client overrides.
  void OnWriteBlocked(TlsConnection* connection) override;
  void OnWriteUnblocked(TlsConnection* connection) override;
  void OnError(TlsConnection* connection, Error error) override;
  void OnRead(TlsConnection* connection, std::vector<uint8_t> block) override;

 private:
  const std::unique_ptr<TlsConnection> connection_;
  std::vector<uint8_t> read_buffer_;
  bool write_blocked_ = false;
  std::vector<CastMessage> message_queue_;
};

}  // namespace channel
}  // namespace cast

#endif  // CAST_COMMON_CHANNEL_CAST_SOCKET_IMPL_H_
