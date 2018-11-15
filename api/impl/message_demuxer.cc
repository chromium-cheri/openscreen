// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/message_demuxer.h"

#include "api/impl/quic/quic_connection.h"
#include "platform/api/logging.h"

namespace openscreen {

// static
MessageDemuxer* MessageDemuxer::instance_;

// static
MessageDemuxer* MessageDemuxer::Get() {
  return instance_;
}

// static
void MessageDemuxer::Set(MessageDemuxer* instance) {
  OSP_DCHECK(!instance_ || !instance);
  instance_ = instance;
}

MessageDemuxer::MessageDemuxer() = default;
MessageDemuxer::~MessageDemuxer() = default;

void MessageDemuxer::WatchMessageType(const IPEndpoint& source,
                                      msgs::Type message_type,
                                      MessageCallback* callback) {
  message_callbacks_[source][message_type] = callback;
}

void MessageDemuxer::StopWatchingMessageType(const IPEndpoint& source,
                                             msgs::Type message_type,
                                             MessageCallback* callback) {
  auto& message_map = message_callbacks_[source];
  auto it = message_map.find(message_type);
  OSP_DCHECK(callback == it->second);
  message_map.erase(it);
}

void MessageDemuxer::OnStreamData(const IPEndpoint& endpoint,
                                  QuicStream* stream,
                                  const char* data,
                                  size_t size) {
  // TODO(btolsch): May also want to discard a single CBOR item (e.g. one
  // map/array/int/etc.) after the message tag if there's no observer to
  // prevent the stream buffer from filling up but being completely unusable
  // (until it's eventually closed).  This would spoil "late" observers though.
  OSP_VLOG(1) << __func__ << ": " << endpoint << " - (" << stream->id() << ", "
              << size << ")";
  auto& stream_map = buffers_[endpoint];
  if (!size) {
    stream_map.erase(stream->id());
    if (stream_map.empty())
      buffers_.erase(endpoint);
    return;
  }
  auto* d = reinterpret_cast<const uint8_t*>(data);
  std::vector<uint8_t>& buffer = stream_map[stream->id()];
  buffer.insert(buffer.end(), d, d + size);

  auto message_map_entry = message_callbacks_.find(endpoint);
  bool handled = false;
  if (message_map_entry != message_callbacks_.end()) {
    OSP_VLOG(1) << "attempting endpoint-specific handling";
    handled = HandleStreamBuffer(endpoint, stream, &message_map_entry->second,
                                 &buffer);
  }
  if (!handled) {
    message_map_entry = message_callbacks_.find(IPEndpoint{{}, 0});
    if (message_map_entry != message_callbacks_.end()) {
      OSP_VLOG(1) << "attempting generic message handling";
      HandleStreamBuffer(endpoint, stream, &message_map_entry->second, &buffer);
    }
  }
}

bool MessageDemuxer::HandleStreamBuffer(
    const IPEndpoint& endpoint,
    QuicStream* stream,
    std::map<msgs::Type, MessageCallback*>* message_callbacks,
    std::vector<uint8_t>* buffer) {
  size_t consumed = 0;
  bool handled = false;
  do {
    consumed = 0;
    auto message_type = static_cast<msgs::Type>((*buffer)[0]);
    auto callback_entry = message_callbacks->find(message_type);
    if (callback_entry == message_callbacks->end())
      break;
    handled = true;
    OSP_VLOG(1) << "handling message type " << static_cast<int>(message_type);
    consumed = callback_entry->second->OnStreamMessage(
        endpoint, stream, message_type, buffer->data() + 1, buffer->size() - 1);
    buffer->erase(buffer->begin(), buffer->begin() + consumed + 1);
  } while (consumed && !buffer->empty());
  return handled;
}

}  // namespace openscreen
