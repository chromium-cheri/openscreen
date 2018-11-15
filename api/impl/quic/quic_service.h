// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_QUIC_QUIC_SERVICE_H_
#define API_IMPL_QUIC_QUIC_SERVICE_H_

#include <map>
#include <memory>
#include <vector>

#include "api/impl/quic/quic_connection.h"
#include "api/impl/quic/quic_connection_factory.h"
#include "base/ip_address.h"
#include "base/macros.h"

namespace openscreen {

class ScopedQuicWriteStream;

// This class serves as the main source of QUIC streams for the rest of the
// library.  It manages connections to other endpoints as well as the lifetime
// of each incoming and outgoing stream.  It works in conjunction with a
// QuicConnectionFactory implementation and MessageDemuxer.
// QuicConnectionFactory provides the actual ability to make a new QUIC
// connection with another endpoint and it also handles creating new QUIC
// connections from a server socket.  Incoming data is given to the QuicService
// by the underlying QUIC implementation (through QuicConnectionFactory) and
// this is in turn handed to MessageDemuxer for routing CBOR messages.
//
// Thw two most significant methods of this class are GetQuicStream and
// GetQuicStreamNow.  Both will return a new QUIC stream to a given endpoint to
// which the caller can write but the former is allowed to be asynchronous.  If
// there isn't currently a connection to the specified endpoint, GetQuicStream
// will start a connection attempt and store the callback for when the
// connection completes.  GetQuicStream simply returns nullptr if there's no
// existing connection.  Additionally, both of these functions return a
// ScopedQuicWriteStream which closes the write end of its stream when it is
// destroyed.
class QuicService final : public QuicConnectionFactory::ServerDelegate {
 public:
  class StreamCallback {
   public:
    virtual ~StreamCallback() = default;

    // This will be called when a connection is made to |endpoint| and a stream
    // is created.  This can be immediate or asynchronous.
    // TODO(btolsch): Error callback if the connection receives an error code
    // before the handshake completes.
    virtual void OnQuicStreamReady(
        const IPEndpoint& endpoint,
        std::unique_ptr<ScopedQuicWriteStream>&& stream) = 0;
  };

  static QuicService* Get();
  static void Set(QuicService* instance);

  QuicService();
  ~QuicService() override;

  void StartServer(IPAddress::Version ip_version, uint16_t port);

  void GetQuicStream(const IPEndpoint& endpoint, StreamCallback* callback);
  std::unique_ptr<ScopedQuicWriteStream> GetQuicStreamNow(
      const IPEndpoint& endpoint);
  void CancelStreamRequest(const IPEndpoint& endpoint,
                           StreamCallback* callback);

  void CloseAllConnections();

 private:
  friend class ScopedQuicWriteStream;

  class ConnectionDelegate final : public QuicConnection::Delegate,
                                   public QuicStream::Delegate {
   public:
    explicit ConnectionDelegate(const IPEndpoint& endpoint);
    ~ConnectionDelegate() override;

    void AddStream(std::unique_ptr<QuicStream>&& stream);
    void RegisterScopedStream(ScopedQuicWriteStream* stream);
    void UnregisterScopedStream(ScopedQuicWriteStream* stream);
    const IPEndpoint& endpoint() const { return endpoint_; }

    // QuicConnection::Delegate overrides.
    void OnCryptoHandshakeComplete(uint64_t connection_id) override;
    void OnIncomingStream(uint64_t connection_id,
                          std::unique_ptr<QuicStream>&& stream) override;
    void OnConnectionClosed(uint64_t connection_id) override;
    QuicStream::Delegate* NextStreamDelegate(uint64_t connection_id) override;

    // QuicStream::Delegate overrides.
    void OnReceived(QuicStream* stream, const char* data, size_t size) override;
    void OnClose(uint64_t stream_id) override;

   private:
    struct InternalStreamPair {
      explicit InternalStreamPair(std::unique_ptr<QuicStream>&& stream);
      ~InternalStreamPair();
      InternalStreamPair(InternalStreamPair&&);
      InternalStreamPair& operator=(InternalStreamPair&&);

      std::unique_ptr<QuicStream> stream;
      ScopedQuicWriteStream* scoped_stream;
    };

    const IPEndpoint endpoint_;
    std::map<uint64_t, InternalStreamPair> streams_;

    DISALLOW_COPY_AND_ASSIGN(ConnectionDelegate);
  };

  struct ConnectionWithDelegate {
    ConnectionWithDelegate(std::unique_ptr<QuicConnection>&& connection,
                           std::unique_ptr<ConnectionDelegate>&& delegate);
    ConnectionWithDelegate(ConnectionWithDelegate&&);
    ~ConnectionWithDelegate();
    ConnectionWithDelegate& operator=(ConnectionWithDelegate&&);

    std::unique_ptr<QuicConnection> connection;
    std::unique_ptr<ConnectionDelegate> delegate;
  };

  void AddQuicStreamRequest(const IPEndpoint& endpoint,
                            StreamCallback* callback);

  // QuicConnectionFactory::ServerDelegate overrides.
  QuicConnection::Delegate* NextConnectionDelegate(
      const IPEndpoint& source) override;
  void OnIncomingConnection(
      std::unique_ptr<QuicConnection>&& connection) override;

  static QuicService* instance_;

  std::unique_ptr<ConnectionDelegate> pending_delegate_;

  std::map<IPEndpoint, std::vector<StreamCallback*>, IPEndpointComparator>
      pending_stream_callbacks_;
  std::map<IPEndpoint, ConnectionWithDelegate, IPEndpointComparator>
      pending_connections_;
  std::map<IPEndpoint, ConnectionWithDelegate, IPEndpointComparator>
      connections_;
};

// This is an RAII class to close the write end of a stream on destruction.  It
// will also be cleared by QuicService if the underlying QUIC stream is closed
// for any reason.
class ScopedQuicWriteStream {
 public:
  explicit ScopedQuicWriteStream(QuicStream* stream);
  ~ScopedQuicWriteStream();

  void SetInternalResetDelegate(QuicService::ConnectionDelegate* delegate);

  explicit operator bool() { return stream_; }

  const QuicStream* stream() const { return stream_; }
  QuicStream* stream() { return stream_; }

  QuicStream* release() {
    QuicStream* result = stream_;
    stream_ = nullptr;
    return result;
  }

 private:
  QuicStream* stream_;
  QuicService::ConnectionDelegate* internal_delegate_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ScopedQuicWriteStream);
};

}  // namespace openscreen

#endif  // API_IMPL_QUIC_QUIC_SERVICE_H_
