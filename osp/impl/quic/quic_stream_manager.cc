// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_stream_manager.h"

#include <memory>
#include <utility>

#include "util/osp_logging.h"

namespace openscreen::osp {

QuicStreamManager::QuicStreamManager(Delegate& delegate,
                                     const std::string& instance_name)
    : delegate_(delegate), instance_name_(instance_name) {}

QuicStreamManager::~QuicStreamManager() {
  void DestroyClosedStreams();
  OSP_CHECK(streams_.empty());
}

void QuicStreamManager::OnReceived(QuicStream* stream, const ByteView& bytes) {
  auto stream_entry = streams_.find(stream->GetStreamId());
  if (stream_entry == streams_.end()) {
    return;
  }

  delegate_.OnDataReceived(instance_id_,
                           stream_entry->second.protocol_connection_id, bytes);
}

void QuicStreamManager::OnClose(uint64_t stream_id) {
  OSP_VLOG << "QUIC stream closed for instance " << instance_name_;
  auto stream_entry = streams_.find(stream_id);
  if (stream_entry == streams_.end()) {
    return;
  }

  QuicProtocolConnection* protocol_connection =
      stream_entry->second.protocol_connection;
  if (protocol_connection) {
    protocol_connection->set_stream(nullptr);
    protocol_connection->OnClose();
  }
  // NOTE: If this OnClose is the result of the read end closing when the write
  // end was already closed, there will likely still be a call to OnReceived.
  // We need to delay actually destroying the stream object until the end of the
  // event loop.
  closed_streams_.push_back(std::move(stream_entry->second));
  streams_.erase(stream_entry);
}

void QuicStreamManager::CreateProtocolConnection(uint64_t stream_id) {
  OSP_CHECK(!pending_connection_);

  pending_connection_ = std::make_unique<QuicProtocolConnection>(
      delegate_, instance_id_, stream_id);
}

std::unique_ptr<QuicProtocolConnection> QuicStreamManager::OnIncomingStream(
    QuicStream* stream) {
  OSP_CHECK(pending_connection_);

  pending_connection_->set_stream(stream);
  AddStreamPair(ServiceStreamPair{stream, pending_connection_->id(),
                                  pending_connection_.get()});
  return std::move(pending_connection_);
}

void QuicStreamManager::AddStreamPair(const ServiceStreamPair& stream_pair) {
  const uint64_t stream_id = stream_pair.stream->GetStreamId();
  streams_.emplace(stream_id, stream_pair);
}

void QuicStreamManager::DropProtocolConnection(
    QuicProtocolConnection* connection) {
  auto stream_entry = streams_.find(connection->stream()->GetStreamId());
  if (stream_entry == streams_.end()) {
    return;
  }

  stream_entry->second.protocol_connection = nullptr;
}

void QuicStreamManager::DestroyClosedStreams() {
  closed_streams_.clear();
}

}  // namespace openscreen::osp
