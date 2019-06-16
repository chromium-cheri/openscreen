// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_SOCKET_H_
#define PLATFORM_API_SOCKET_H_

#include <cstdint>
#include <memory>
#include <string>

#include "osp_base/error.h"
#include "osp_base/ip_address.h"
#include "osp_base/macros.h"
#include "platform/api/network_interface.h"

namespace openscreen {
namespace platform {

class Socket;

// Platform-specific deleter of a Socket instance returned by
// Socket::Create().
struct SocketDeleter {
  void operator()(Socket* socket) const;
};

using SocketUniquePtr = std::unique_ptr<Socket, SocketDeleter>;

// An open UDP socket for sending/receiving datagrams to/from either specific
// endpoints or over IP multicast.
//
// Usage: The socket is created and opened by calling the Create() method. This
// returns a unique pointer that auto-closes/destroys the socket when it goes
// out-of-scope.
//
// Platform implementation note: There must only be one platform-specific
// implementation of Socket linked into the library/application. For that
// reason, none of the methods here are declared virtual (i.e., the overhead is
// pure waste). However, Socket can be subclassed to include all extra
// private state, such as OS-specific handles. See SocketPosix for a
// reference implementation.
class Socket {
 public:
  // Constants used to dictate why the socket is being closed.
  enum class CloseReason {
    kUnknown = 0,
    kClosedByPeer,
    kAbortedByPeer,
    kInvalidMessage,
    kTooLongInactive,
  };

  // Constants used to specify how we want packets sent from this socket.
  enum class DscpMode : uint8_t {
    // Default value set by the system on creation of a new socket.
    kUnspecified = 0x0,

    // Mode for Audio only.
    kAudioOnly = 0xb8,

    // Mode for Audio + Video.
    kAudioVideo = 0x88,

    // Mode for low priority operations such as trace log data.
    kLowPriority = 0x20
  };

  enum class Type { Udp, Tcp };

  static constexpr int kMaxMessageSize = 1 << 16;
  struct Message : std::array<uint8_t, kMaxMessageSize> {
    size_t length;
    IPEndpoint source;
    IPEndpoint destination;

    Socket* socket;
    // TODO(jophba): same as length???
    size_t num_bytes_received;
  };

  class Delegate {
   public:
    // Provides a unique ID for use creating new sockets.
    virtual const std::string& GetNewSocketId() = 0;

    // Called when a |socket| is created or accepted.
    virtual void OnAccepted(std::unique_ptr<Socket> socket) = 0;

    // Called when |socket| is closed.
    virtual void OnClosed(Socket* socket) = 0;

    // Called when a |message| arrives on |socket|.
    virtual void OnMessage(Socket* socket, const Message& message) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  using Version = IPAddress::Version;

  // Creates a new, scoped Socket within the IPv4 or IPv6 family.
  static ErrorOr<SocketUniquePtr> Create(Version version,
                                         Type type,
                                         std::string id,
                                         Delegate* delegate);

  // TODO(jophba): do we actually need id?
  static ErrorOr<SocketUniquePtr> Create(Version version, Type type);

  // Returns true if |socket| belongs to the IPv4/IPv6 address family.
  bool IsIPv4() const;
  bool IsIPv6() const;

  // Sets the socket for address reuse, binds to the address/port.
  Error Bind(const IPEndpoint& local_endpoint);

  // Closes the socket, with a specified reason.
  Error Close(CloseReason reason);

  // Sets the device to use for outgoing multicast packets on the socket.
  Error SetMulticastOutboundInterface(NetworkInterfaceIndex ifindex);

  // Joins to the multicast group at the given address, using the specified
  // interface.
  Error JoinMulticastGroup(const IPAddress& address,
                           NetworkInterfaceIndex ifindex);

  // Start reading data. Delegate::OnMessage is called when new data arrives.
  Error ReadRepeatedly();

  // Performs a non-blocking read on the socket, returning the number of bytes
  // received. Note that a non-Error return value of 0 is a valid result,
  // indicating an empty message has been received. Also note that
  // Error::Code::kAgain might be returned if there is no message currently
  // ready for receive, which can be expected during normal operation. |source|
  // and |destination| are optional output arguments that provide the
  // source of the message and its intended destination, respectively.
  ErrorOr<Message> ReceiveMessage();

  // Sends a message and returns the number of bytes sent, on success.
  // Error::Code::kAgain might be returned to indicate the operation would
  // block, which can be expected during normal operation.
  Error SendMessage(const Message& message);

  // Sets the DSCP value to use for all messages sent from this socket.
  Error SetDscp(DscpMode state);

  // Returns the unique identifier for this socket.
  const std::string& id() const { return id_; }

 protected:
  Delegate* delegate() const { return delegate_; }

  Socket(std::string id, Delegate* delegate);
  ~Socket();

  const std::string id_;
  Delegate* const delegate_;

 private:
  OSP_DISALLOW_COPY_AND_ASSIGN(Socket);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_SOCKET_H_
