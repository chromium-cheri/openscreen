// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_UDP_SOCKET_H_
#define PLATFORM_API_UDP_SOCKET_H_

#include <cstdint>
#include <functional>
#include <memory>

#include "platform/api/network_interface.h"
#include "platform/api/udp_read_callback.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"
#include "platform/base/macros.h"

namespace openscreen {
namespace platform {

class UdpSocket;

using UdpSocketUniquePtr = std::unique_ptr<UdpSocket>;

// An open UDP socket for sending/receiving datagrams to/from either specific
// endpoints or over IP multicast.
//
// Usage: The socket is created and opened by calling the Create() method. This
// returns a unique pointer that auto-closes/destroys the socket when it goes
// out-of-scope.
//
// Platform implementation note: There must only be one platform-specific
// implementation of UdpSocket linked into the library/application. For that
// reason, none of the methods here are declared virtual (i.e., the overhead is
// pure waste). However, UdpSocket can be subclassed to include all extra
// private state, such as OS-specific handles. See UdpSocketPosix for a
// reference implementation.
class UdpSocket {
 public:
  virtual ~UdpSocket();

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

  // Many embedders only have access to asynchronous operations on UDPSockets,
  // even if the operating system provides synchronous operations.
  class Observer {
   public:
    virtual void OnBindComplete(UdpSocket* socket, Error error) = 0;
    virtual void OnSetMulticastComplete(UdpSocket* socket, Error error) = 0;
    virtual void OnJoinMulticastComplete(UdpSocket* socket, Error error) = 0;
    virtual void OnSendMessageComplete(UdpSocket* socket, Error error) = 0;
    virtual void OnSetDscpComplete(UdpSocket* socket, Error error) = 0;

    virtual void OnDestroyed(UdpSocket* socket) = 0;
    virtual void OnMessage(UdpSocket* socket, UdpPacket message) = 0;
  };

  using Version = IPAddress::Version;

  // Creates a new, scoped UdpSocket within the IPv4 or IPv6 family. This method
  // must be defined in the platform-level implementation.
  static ErrorOr<UdpSocketUniquePtr> Create(const IPEndpoint& endpoint);

  // Returns true if |socket| belongs to the IPv4/IPv6 address family.
  virtual bool IsIPv4() const = 0;
  virtual bool IsIPv6() const = 0;

  // Binds to the address specified in the constructor.
  virtual Error Bind() = 0;

  // Sets the device to use for outgoing multicast packets on the socket.
  virtual Error SetMulticastOutboundInterface(
      NetworkInterfaceIndex ifindex) = 0;

  // Joins to the multicast group at the given address, using the specified
  // interface.
  virtual Error JoinMulticastGroup(const IPAddress& address,
                                   NetworkInterfaceIndex ifindex) = 0;

  // Sends a message and returns the number of bytes sent, on success.
  // Error::Code::kAgain might be returned to indicate the operation would
  // block, which can be expected during normal operation.
  virtual Error SendMessage(const void* data,
                            size_t length,
                            const IPEndpoint& dest) = 0;

  // Sets the DSCP value to use for all messages sent from this socket.
  virtual Error SetDscp(DscpMode state) = 0;

 protected:
  UdpSocket(const Observer* observer);

 private:
  Observer* const observer_ = nullptr;

  OSP_DISALLOW_COPY_AND_ASSIGN(UdpSocket);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_UDP_SOCKET_H_
