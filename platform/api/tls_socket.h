// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TLS_SOCKET_H_
#define PLATFORM_API_TLS_SOCKET_H_

#include <cstdint>
#include <memory>
#include <string>

#include "absl/types/optional.h"
#include "platform/api/network_interface.h"
#include "platform/api/tls_packet.h"
#include "platform/api/tls_socket_creds.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"
#include "platform/base/macros.h"

namespace openscreen {
namespace platform {

class TlsSocket {
 public:
  enum class CloseReason {
    kUnknown = 0,
    kClosedByPeer,
    kAbortedByPeer,
    kInvalidMessage,
    kTooLongInactive,
  };

  class Client {
   public:
    // Provides a unique ID for use by the TlsSocketFactory.
    virtual const std::string& GetNewSocketId() = 0;

    // Called when a |socket| is created or accepted.
    virtual void OnAccepted(std::unique_ptr<TlsSocket> socket) = 0;

    // Called when |socket| is closed.
    virtual void OnClosed(TlsSocket* socket, CloseReason reason) = 0;

    // Called when |socket| experiences an error, such as a read error.
    virtual void OnError(TlsSocket* socket, Error error) = 0;

    // Called when a |message| arrives on |socket|.
    virtual void OnMessage(TlsSocket* socket, const TlsPacket& message) = 0;

   protected:
    virtual ~Client() = default;
  };

  // Returns true if |socket| belongs to the IPv4/IPv6 address family.
  virtual bool IsIPv4() const = 0;
  virtual bool IsIPv6() const = 0;

  // Closes this socket. Delegate::OnClosed is called when complete.
  virtual void Close(CloseReason reason) = 0;

  // Sends a message.
  virtual void Write(const TlsPacket& message) = 0;

  // Get the connected remote address.
  virtual const IPEndpoint& remote_address() const = 0;

  // Returns the current connection state of the socket.
  virtual SocketState state() const = 0;

  // Returns the unique identifier of the parent server socket.
  virtual absl::optional<std::string> parent_server_socket_id() const = 0;

  // Returns the unique identifier for this socket.
  virtual const std::string& id() const = 0;

 protected:
  TlsSocket(Client* client) : client_(client) {}

  Client* client() const { return client_; }

 private:
  Client* client_;

  OSP_DISALLOW_COPY_AND_ASSIGN(TlsSocket);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_TLS_SOCKET_H_
