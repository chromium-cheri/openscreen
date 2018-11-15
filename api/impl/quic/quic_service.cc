// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/quic/quic_service.h"

#include <algorithm>

#include "api/impl/message_demuxer.h"
#include "api/impl/quic/quic_connection_factory.h"
#include "base/make_unique.h"
#include "platform/api/logging.h"

namespace openscreen {

// static
QuicService* QuicService::instance_;

// static
QuicService* QuicService::Get() {
  return instance_;
}

// static
void QuicService::Set(QuicService* instance) {
  OSP_DCHECK(!instance_ || !instance);
  instance_ = instance;
}

QuicService::QuicService() = default;
QuicService::~QuicService() = default;

void QuicService::StartServer(IPAddress::Version ip_version, uint16_t port) {
  QuicConnectionFactory::Get()->SetServerDelegate(this, ip_version, port);
}

void QuicService::GetQuicStream(const IPEndpoint& endpoint,
                                StreamCallback* callback) {
  auto entry = connections_.find(endpoint);
  if (entry == connections_.end()) {
    OSP_VLOG(1) << __func__ << ": queueing " << endpoint;
    AddQuicStreamRequest(endpoint, callback);
  } else {
    OSP_VLOG(1) << __func__ << ": immediate " << endpoint;
    std::unique_ptr<QuicStream> stream =
        entry->second.connection->MakeOutgoingStream(
            entry->second.delegate.get());
    auto* stream_ptr = stream.get();
    entry->second.delegate->AddStream(std::move(stream));
    ConnectionDelegate* connection = entry->second.delegate.get();
    auto scoped_stream = MakeUnique<ScopedQuicWriteStream>(stream_ptr);
    ScopedQuicWriteStream* ptr = scoped_stream.get();
    callback->OnQuicStreamReady(endpoint, std::move(scoped_stream));
    if (!scoped_stream)
      connection->RegisterScopedStream(ptr);
  }
}

std::unique_ptr<ScopedQuicWriteStream> QuicService::GetQuicStreamNow(
    const IPEndpoint& endpoint) {
  auto entry = connections_.find(endpoint);
  if (entry == connections_.end()) {
    return nullptr;
  } else {
    std::unique_ptr<QuicStream> stream =
        entry->second.connection->MakeOutgoingStream(
            entry->second.delegate.get());
    auto* stream_ptr = stream.get();
    entry->second.delegate->AddStream(std::move(stream));
    ConnectionDelegate* connection = entry->second.delegate.get();
    auto result = MakeUnique<ScopedQuicWriteStream>(stream_ptr);
    connection->RegisterScopedStream(result.get());
    return result;
  }
}

void QuicService::CancelStreamRequest(const IPEndpoint& endpoint,
                                      StreamCallback* callback) {
  OSP_VLOG(1) << __func__ << ": " << endpoint;
  auto callback_entry = pending_stream_callbacks_.find(endpoint);
  if (callback_entry == pending_stream_callbacks_.end())
    return;
  std::vector<StreamCallback*>& endpoint_callbacks = callback_entry->second;
  endpoint_callbacks.erase(std::remove(endpoint_callbacks.begin(),
                                       endpoint_callbacks.end(), callback),
                           endpoint_callbacks.end());
  if (endpoint_callbacks.empty())
    pending_stream_callbacks_.erase(callback_entry);
}

void QuicService::CloseAllConnections() {
  OSP_VLOG(1) << "closing all connections";
  for (auto& conn : pending_connections_)
    conn.second.connection->Close();
  pending_connections_.clear();
  for (auto& conn : connections_)
    conn.second.connection->Close();
  connections_.clear();
  pending_stream_callbacks_.clear();
}

QuicService::ConnectionDelegate::ConnectionDelegate(const IPEndpoint& endpoint)
    : endpoint_(endpoint) {}
QuicService::ConnectionDelegate::~ConnectionDelegate() = default;

void QuicService::ConnectionDelegate::AddStream(
    std::unique_ptr<QuicStream>&& stream) {
  uint64_t stream_id = stream->id();
  streams_.emplace(stream_id, InternalStreamPair(std::move(stream)));
}

void QuicService::ConnectionDelegate::RegisterScopedStream(
    ScopedQuicWriteStream* stream) {
  auto entry = streams_.find(stream->stream()->id());
  OSP_DCHECK(entry != streams_.end());
  OSP_DCHECK(!entry->second.scoped_stream);
  entry->second.scoped_stream = stream;
  stream->SetInternalResetDelegate(this);
}

void QuicService::ConnectionDelegate::UnregisterScopedStream(
    ScopedQuicWriteStream* stream) {
  auto entry = streams_.find(stream->stream()->id());
  OSP_DCHECK(entry != streams_.end());
  OSP_DCHECK(entry->second.scoped_stream);
  entry->second.scoped_stream = nullptr;
}

void QuicService::ConnectionDelegate::OnCryptoHandshakeComplete(
    uint64_t connection_id) {
  OSP_VLOG(1) << __func__ << ": " << endpoint_ << " " << connection_id;
  auto* service = QuicService::Get();
  auto entry = service->pending_connections_.find(endpoint_);
  auto emplace_entry =
      service->connections_.emplace(endpoint_, std::move(entry->second));
  QuicConnection* connection = emplace_entry.first->second.connection.get();
  service->pending_connections_.erase(entry);
  auto callback_entry = service->pending_stream_callbacks_.find(endpoint_);
  if (callback_entry != service->pending_stream_callbacks_.end()) {
    OSP_VLOG(1) << "...with pending stream callbacks";
    for (auto* callback : callback_entry->second) {
      std::unique_ptr<QuicStream> stream = connection->MakeOutgoingStream(this);
      QuicStream* stream_ptr = stream.get();
      auto maybe_entry = streams_.emplace(stream_ptr->id(), std::move(stream));
      auto scoped_stream = MakeUnique<ScopedQuicWriteStream>(stream_ptr);
      ScopedQuicWriteStream* ptr = scoped_stream.get();
      callback->OnQuicStreamReady(endpoint_, std::move(scoped_stream));
      if (!scoped_stream)
        maybe_entry.first->second.scoped_stream = ptr;
    }
    service->pending_stream_callbacks_.erase(callback_entry);
  }
}

void QuicService::ConnectionDelegate::OnIncomingStream(
    uint64_t connection_id,
    std::unique_ptr<QuicStream>&& stream) {
  OSP_VLOG(1) << __func__ << ": " << endpoint_ << " " << connection_id << ":"
              << stream->id();
  uint64_t stream_id = stream->id();
  streams_.emplace(stream_id, InternalStreamPair(std::move(stream)));
}

void QuicService::ConnectionDelegate::OnConnectionClosed(
    uint64_t connection_id) {
  OSP_VLOG(1) << "connection closed: " << connection_id;
  auto* service = QuicService::Get();
  auto entry = service->connections_.find(endpoint_);
  if (entry != service->connections_.end()) {
    QuicConnectionFactory::Get()->OnConnectionClosed(
        entry->second.connection.get());
    return;
  }
  auto pending_entry = service->pending_connections_.find(endpoint_);
  QuicConnectionFactory::Get()->OnConnectionClosed(
      pending_entry->second.connection.get());
}

QuicStream::Delegate* QuicService::ConnectionDelegate::NextStreamDelegate(
    uint64_t connection_id) {
  return this;
}

void QuicService::ConnectionDelegate::OnReceived(QuicStream* stream,
                                                 const char* data,
                                                 size_t size) {
  MessageDemuxer::Get()->OnStreamData(endpoint_, stream, data, size);
  if (!size) {
    OSP_VLOG(1) << "stream fin: " << stream->id();
    streams_.erase(stream->id());
  }
}

void QuicService::ConnectionDelegate::OnClose(uint64_t stream_id) {
  OSP_VLOG(1) << "stream closed: " << stream_id;
  auto entry = streams_.find(stream_id);
  if (entry != streams_.end() && entry->second.scoped_stream)
    entry->second.scoped_stream->release();
}

QuicService::ConnectionDelegate::InternalStreamPair::InternalStreamPair(
    std::unique_ptr<QuicStream>&& stream)
    : stream(std::move(stream)), scoped_stream(nullptr) {}
QuicService::ConnectionDelegate::InternalStreamPair::~InternalStreamPair() =
    default;
QuicService::ConnectionDelegate::InternalStreamPair::InternalStreamPair(
    InternalStreamPair&&) = default;
QuicService::ConnectionDelegate::InternalStreamPair&
QuicService::ConnectionDelegate::InternalStreamPair::operator=(
    InternalStreamPair&&) = default;

QuicService::ConnectionWithDelegate::ConnectionWithDelegate(
    std::unique_ptr<QuicConnection>&& connection,
    std::unique_ptr<ConnectionDelegate>&& delegate)
    : connection(std::move(connection)), delegate(std::move(delegate)) {}
QuicService::ConnectionWithDelegate::ConnectionWithDelegate(
    ConnectionWithDelegate&&) = default;
QuicService::ConnectionWithDelegate::~ConnectionWithDelegate() = default;
QuicService::ConnectionWithDelegate& QuicService::ConnectionWithDelegate::
operator=(ConnectionWithDelegate&&) = default;

void QuicService::AddQuicStreamRequest(const IPEndpoint& endpoint,
                                       StreamCallback* callback) {
  auto& callbacks = pending_stream_callbacks_[endpoint];
  callbacks.push_back(callback);
  if (callbacks.size() == 1) {
    auto delegate = MakeUnique<ConnectionDelegate>(endpoint);
    auto connection =
        QuicConnectionFactory::Get()->Connect(endpoint, delegate.get());
    pending_connections_.emplace(
        endpoint,
        ConnectionWithDelegate{std::move(connection), std::move(delegate)});
  }
}

QuicConnection::Delegate* QuicService::NextConnectionDelegate(
    const IPEndpoint& source) {
  OSP_DCHECK(!pending_delegate_);
  pending_delegate_ = MakeUnique<ConnectionDelegate>(source);
  return pending_delegate_.get();
}

void QuicService::OnIncomingConnection(
    std::unique_ptr<QuicConnection>&& connection) {
  OSP_DCHECK(pending_delegate_);
  IPEndpoint endpoint = pending_delegate_->endpoint();
  OSP_VLOG(1) << __func__ << ": " << endpoint;
  pending_connections_.emplace(
      endpoint, ConnectionWithDelegate{std::move(connection),
                                       std::move(pending_delegate_)});
}

ScopedQuicWriteStream::ScopedQuicWriteStream(QuicStream* stream)
    : stream_(stream) {}

ScopedQuicWriteStream::~ScopedQuicWriteStream() {
  if (stream_) {
    stream_->CloseWriteEnd();
    internal_delegate_->UnregisterScopedStream(this);
  }
}

void ScopedQuicWriteStream::SetInternalResetDelegate(
    QuicService::ConnectionDelegate* delegate) {
  OSP_DCHECK(!internal_delegate_);
  internal_delegate_ = delegate;
}

}  // namespace openscreen
