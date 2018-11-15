// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_QUIC_TESTING_FAKE_QUIC_CONNECTION_FACTORY_H_
#define API_IMPL_QUIC_TESTING_FAKE_QUIC_CONNECTION_FACTORY_H_

#include <vector>

#include "api/impl/message_demuxer.h"
#include "api/impl/quic/quic_connection_factory.h"
#include "api/impl/quic/testing/fake_quic_connection.h"

namespace openscreen {

class FakeQuicConnectionFactory;

class RunAsReceiver {
 public:
  explicit RunAsReceiver(FakeQuicConnectionFactory* factory);
  ~RunAsReceiver();

 private:
  FakeQuicConnectionFactory* const factory_;
};

class FakeQuicConnectionFactory final : public QuicConnectionFactory {
 public:
  FakeQuicConnectionFactory(const IPEndpoint& local_endpoint,
                            const IPEndpoint& remote_endpoint,
                            MessageDemuxer* local_demuxer,
                            MessageDemuxer* remote_demuxer);
  ~FakeQuicConnectionFactory() override;

  void SwitchEndpointContexts();
  void RunTasksUntilIdle();

  void SetServerDelegate(ServerDelegate* delegate,
                         IPAddress::Version ip_version,
                         uint16_t port) override;
  void RunTasks() override;

  std::unique_ptr<QuicConnection> Connect(
      const IPEndpoint& endpoint,
      QuicConnection::Delegate* connection_delegate) override;
  void OnConnectionClosed(QuicConnection* connection) override;

 private:
  struct ConnectionWithIdentity {
    IPEndpoint endpoints[2];
    FakeQuicConnection* connection;
  };

  ServerDelegate* server_delegate_ = nullptr;
  MessageDemuxer* local_demuxer_;
  MessageDemuxer* remote_demuxer_;
  IPEndpoint local_endpoint_;
  IPEndpoint remote_endpoint_;
  bool idle_ = true;
  uint8_t remote_endpoint_index_ = 1;
  uint64_t next_connection_id_ = 1;
  std::vector<ConnectionWithIdentity> pending_connections_;
  std::vector<ConnectionWithIdentity> connections_;
};

}  // namespace openscreen

#endif  // API_IMPL_QUIC_TESTING_FAKE_QUIC_CONNECTION_FACTORY_H_
