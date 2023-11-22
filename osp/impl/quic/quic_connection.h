// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_CONNECTION_H_
#define OSP_IMPL_QUIC_QUIC_CONNECTION_H_

#include <memory>
#include <string>
#include <vector>

#include "platform/api/udp_socket.h"

namespace openscreen::osp {

class QuicStream {
 public:
  class Delegate {
   public:

    virtual void OnReceived(QuicStream* stream,
                            const char* data,
                            size_t data_size) = 0;
    virtual void OnClose(uint64_t stream_id) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  explicit QuicStream(Delegate* delegate) : delegate_(delegate) {}
  virtual ~QuicStream() = default;

  virtual uint64_t GetStreamId() = 0;
  virtual void Write(const uint8_t* data, size_t data_size) = 0;
  virtual void CloseWriteEnd() = 0;

 protected:
  Delegate* const delegate_;
};

class QuicConnection : public UdpSocket::Client {
 public:
  class Delegate {
   public:
    // Called when the QUIC handshake has successfully completed.
    virtual void OnCryptoHandshakeComplete(std::string connection_id) = 0;

    // Called when a new stream on this connection is initiated by the other
    // endpoint.  |stream| will use a delegate returned by NextStreamDelegate.
    virtual void OnIncomingStream(std::string connection_id,
                                  QuicStream* stream) = 0;

    // Called when the QUIC connection was closed.  The QuicConnection should
    // not be destroyed immediately, because the QUIC implementation will still
    // reference it briefly.  Instead, it should be destroyed during the next
    // event loop.
    // TODO(btolsch): Hopefully this can be changed with future QUIC
    // implementations.
    virtual void OnConnectionClosed(std::string connection_id) = 0;

    // This is used to get a QuicStream::Delegate for an incoming stream, which
    // will be returned via OnIncomingStream immediately after this call.
    virtual QuicStream::Delegate* NextStreamDelegate(std::string connection_id,
                                                     uint64_t stream_id) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  explicit QuicConnection(Delegate* delegate) : delegate_(delegate) {}
  virtual ~QuicConnection() = default;

  virtual QuicStream* MakeOutgoingStream(QuicStream::Delegate* delegate) = 0;
  virtual void Close() = 0;

 protected:
  Delegate* const delegate_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_CONNECTION_H_
