// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/quic/testing/fake_quic_connection_factory.h"

#include "base/make_unique.h"
#include "platform/api/logging.h"

namespace openscreen {

FakeQuicConnectionFactoryBridge::FakeQuicConnectionFactoryBridge(
    const IPEndpoint& controller_endpoint)
    : controller_endpoint_(controller_endpoint) {}

void FakeQuicConnectionFactoryBridge::OnConnectionClosed(
    QuicConnection* connection) {
  if (connection == connections_.controller) {
    connections_.controller = nullptr;
    return;
  } else if (connection == connections_.receiver) {
    connections_.receiver = nullptr;
    return;
  }
  OSP_DCHECK(false) << "reporting an unknown connection as closed";
}

void FakeQuicConnectionFactoryBridge::OnOutgoingStream(
    QuicConnection* connection,
    QuicStream* stream) {
  if (connection == connections_.controller) {
    connections_.receiver->delegate()->OnIncomingStream(
        connections_.receiver->id(),
        connections_.receiver->MakeIncomingStream());
  } else if (connection == connections_.receiver) {
    connections_.controller->delegate()->OnIncomingStream(
        connections_.controller->id(),
        connections_.controller->MakeIncomingStream());
  }
}

void FakeQuicConnectionFactoryBridge::SetServerDelegate(
    QuicConnectionFactory::ServerDelegate* delegate,
    const IPEndpoint& endpoint) {
  OSP_DCHECK(!delegate_ || !delegate);
  delegate_ = delegate;
  receiver_endpoint_ = endpoint;
}

void FakeQuicConnectionFactoryBridge::RunTasks() {
  idle_ = true;
  if (!connections_.controller || !connections_.receiver)
    return;
  if (connections_pending_) {
    idle_ = false;
    connections_.receiver->delegate()->OnCryptoHandshakeComplete(
        connections_.receiver->id());
    connections_.controller->delegate()->OnCryptoHandshakeComplete(
        connections_.controller->id());
    connections_pending_ = false;
  } else {
    OSP_DCHECK_EQ(connections_.controller->streams().size(),
                  connections_.receiver->streams().size());
    size_t num_streams = connections_.controller->streams().size();
    auto& controller_streams = connections_.controller->streams();
    auto& receiver_streams = connections_.receiver->streams();
    auto controller_it = controller_streams.begin();
    auto receiver_it = receiver_streams.begin();
    for (size_t i = 0; i < num_streams; ++i) {
      auto* controller_stream = controller_it->second;
      auto* receiver_stream = receiver_it->second;
      std::vector<uint8_t> written_data = controller_stream->TakeWrittenData();
      OSP_DCHECK(controller_stream->TakeReceivedData().empty());
      if (written_data.size()) {
        idle_ = false;
        receiver_stream->delegate()->OnReceived(
            receiver_stream, reinterpret_cast<const char*>(written_data.data()),
            written_data.size());
      }
      if (controller_stream->write_end_closed())
        receiver_stream->CloseReadEnd();
      written_data = receiver_stream->TakeWrittenData();
      OSP_DCHECK(receiver_stream->TakeReceivedData().empty());
      if (written_data.size()) {
        idle_ = false;
        controller_stream->delegate()->OnReceived(
            controller_stream,
            reinterpret_cast<const char*>(written_data.data()),
            written_data.size());
      }
      if (receiver_stream->write_end_closed())
        controller_stream->CloseReadEnd();
      if (controller_stream->write_end_closed() &&
          controller_stream->read_end_closed()) {
        controller_stream->delegate()->OnClose(controller_stream->id());
        controller_stream->delegate()->OnReceived(controller_stream, nullptr,
                                                  0);
        controller_it = controller_streams.erase(controller_it);
      } else {
        ++controller_it;
      }
      if (receiver_stream->write_end_closed() &&
          receiver_stream->read_end_closed()) {
        receiver_stream->delegate()->OnClose(receiver_stream->id());
        receiver_stream->delegate()->OnReceived(receiver_stream, nullptr, 0);
        receiver_it = receiver_streams.erase(receiver_it);
      } else {
        ++receiver_it;
      }
    }
  }
}

std::unique_ptr<QuicConnection> FakeQuicConnectionFactoryBridge::Connect(
    const IPEndpoint& endpoint,
    QuicConnection::Delegate* connection_delegate) {
  if (endpoint.address != receiver_endpoint_.address ||
      endpoint.port != receiver_endpoint_.port) {
    return nullptr;
  }
  OSP_DCHECK(!connections_.controller);
  OSP_DCHECK(!connections_.receiver);
  auto controller_connection = MakeUnique<FakeQuicConnection>(
      this, next_connection_id_++, connection_delegate);
  connections_.controller = controller_connection.get();

  auto receiver_connection = MakeUnique<FakeQuicConnection>(
      this, next_connection_id_++,
      delegate_->NextConnectionDelegate(controller_endpoint_));
  connections_.receiver = receiver_connection.get();
  delegate_->OnIncomingConnection(std::move(receiver_connection));
  return controller_connection;
}

FakeClientQuicConnectionFactory::FakeClientQuicConnectionFactory(
    FakeQuicConnectionFactoryBridge* bridge)
    : bridge_(bridge) {}
FakeClientQuicConnectionFactory::~FakeClientQuicConnectionFactory() = default;

void FakeClientQuicConnectionFactory::SetServerDelegate(
    ServerDelegate* delegate,
    const std::vector<IPEndpoint>& endpoints) {
  OSP_DCHECK(false) << "don't call SetServerDelegate from QuicClient side";
}

void FakeClientQuicConnectionFactory::RunTasks() {
  bridge_->RunTasks();
}

std::unique_ptr<QuicConnection> FakeClientQuicConnectionFactory::Connect(
    const IPEndpoint& endpoint,
    QuicConnection::Delegate* connection_delegate) {
  return bridge_->Connect(endpoint, connection_delegate);
}

FakeServerQuicConnectionFactory::FakeServerQuicConnectionFactory(
    FakeQuicConnectionFactoryBridge* bridge)
    : bridge_(bridge) {}
FakeServerQuicConnectionFactory::~FakeServerQuicConnectionFactory() = default;

void FakeServerQuicConnectionFactory::SetServerDelegate(
    ServerDelegate* delegate,
    const std::vector<IPEndpoint>& endpoints) {
  if (delegate) {
    OSP_DCHECK_EQ(1u, endpoints.size())
        << "fake bridge doesn't support multiple server endpoints";
  }
  bridge_->SetServerDelegate(delegate,
                             endpoints.empty() ? IPEndpoint{} : endpoints[0]);
}

void FakeServerQuicConnectionFactory::RunTasks() {
  bridge_->RunTasks();
}

std::unique_ptr<QuicConnection> FakeServerQuicConnectionFactory::Connect(
    const IPEndpoint& endpoint,
    QuicConnection::Delegate* connection_delegate) {
  OSP_DCHECK(false) << "don't call Connect() from QuicServer side";
  return nullptr;
}

}  // namespace openscreen
