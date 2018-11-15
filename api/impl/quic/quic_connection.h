// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_QUIC_QUIC_CONNECTION_H_
#define API_IMPL_QUIC_QUIC_CONNECTION_H_

#include <memory>
#include <vector>

#include "platform/api/socket.h"
#include "platform/base/event_loop.h"

namespace openscreen {

class QuicStream {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void OnReceived(QuicStream* stream,
                            const char* data,
                            size_t size) = 0;
    virtual void OnClose(uint64_t stream_id) = 0;
  };

  QuicStream(Delegate* delegate, uint64_t id) : delegate_(delegate), id_(id) {}
  virtual ~QuicStream() = default;

  uint64_t id() const { return id_; }
  virtual void Write(const uint8_t* data, size_t size) = 0;
  virtual void CloseWriteEnd() = 0;

 protected:
  Delegate* const delegate_;
  uint64_t id_;
};

class QuicConnection {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void OnCryptoHandshakeComplete(uint64_t connection_id) = 0;
    virtual void OnIncomingStream(uint64_t connection_id,
                                  std::unique_ptr<QuicStream>&& stream) = 0;
    virtual void OnConnectionClosed(uint64_t connection_id) = 0;

    virtual QuicStream::Delegate* NextStreamDelegate(
        uint64_t connection_id) = 0;
  };

  explicit QuicConnection(Delegate* delegate) : delegate_(delegate) {}
  virtual ~QuicConnection() = default;

  virtual void OnDataReceived(const platform::ReceivedData& data) = 0;
  virtual std::unique_ptr<QuicStream> MakeOutgoingStream(
      QuicStream::Delegate* delegate) = 0;
  virtual void Close() = 0;

 protected:
  Delegate* const delegate_;
};

}  // namespace openscreen

#endif  // API_IMPL_QUIC_QUIC_CONNECTION_H_
