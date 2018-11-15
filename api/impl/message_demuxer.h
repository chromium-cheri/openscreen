// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_MESSAGE_DEMUXER_H_
#define API_IMPL_MESSAGE_DEMUXER_H_

#include <map>
#include <vector>

#include "base/ip_address.h"
#include "msgs/osp_messages.h"

namespace openscreen {

class QuicStream;

// This class separates QUIC stream data into CBOR messages by reading a type
// prefix from the stream and passes those messages to any callback matching the
// source endpoint and message type.  If there is no callback for a given
// message type, it will also try a global message listener (one registered with
// an all-0 IPEndpoint.
class MessageDemuxer {
 public:
  class MessageCallback {
   public:
    virtual ~MessageCallback() = default;

    virtual size_t OnStreamMessage(const IPEndpoint& source,
                                   QuicStream* stream,
                                   msgs::Type message_type,
                                   const uint8_t* buffer,
                                   size_t size) = 0;
  };

  static MessageDemuxer* Get();
  static void Set(MessageDemuxer* instance);

  MessageDemuxer();
  ~MessageDemuxer();

  void WatchMessageType(const IPEndpoint& source,
                        msgs::Type message_type,
                        MessageCallback* callback);
  void StopWatchingMessageType(const IPEndpoint& source,
                               msgs::Type message_type,
                               MessageCallback* callback);

  void OnStreamData(const IPEndpoint& endpoint,
                    QuicStream* stream,
                    const char* data,
                    size_t size);

 private:
  static MessageDemuxer* instance_;

  bool HandleStreamBuffer(
      const IPEndpoint& endpoint,
      QuicStream* stream,
      std::map<msgs::Type, MessageCallback*>* message_callbacks,
      std::vector<uint8_t>* buffer);

  std::map<IPEndpoint,
           std::map<msgs::Type, MessageCallback*>,
           IPEndpointComparator>
      message_callbacks_;
  std::map<IPEndpoint,
           std::map<uint64_t, std::vector<uint8_t>>,
           IPEndpointComparator>
      buffers_;
};

}  // namespace openscreen

#endif  // API_IMPL_MESSAGE_DEMUXER_H_
