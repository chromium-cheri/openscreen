// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_SOCKET_H_
#define PLATFORM_API_SOCKET_H_

#include <cstdint>
#include <memory>

#include "base/ip_address.h"
#include "base/macros.h"
#include "platform/api/network_interface.h"
#include "third_party/abseil/src/absl/types/optional.h"

namespace openscreen {
namespace platform {

class UdpSocket;

// Platform-specific deleter of a UdpSocket instance returned by
// UdpSocket::Create().
struct UdpSocketDeleter {
  void operator()(UdpSocket* socket) const;
};

using UdpSocketUniquePtr = std::unique_ptr<UdpSocket, UdpSocketDeleter>;

// An open UDP socket for sending/receiving datagrams to/from either specific
// endpoints or over IP multicast.
//
// Usage: The socket is created and opened by calling the Create() method. This
// returns a unique pointer that auto-closes/destroys the socket when it goes
// out-of-scope. All non-const operations on the socket return true to indicate
// success, and false on error. In the latter case, GetLastError() and
// GetLastErrorString() (see error.h) can be used to retrieve more detail about
// the error.
//
// Platform implementation note: There must only be one platform-specific
// implementation of UdpSocket linked into the library/application. For that
// reason, none of the methods here are declared virtual (i.e., the overhead is
// pure waste). However, UdpSocket can be subclassed to include all extra
// private state, such as OS-specific handles. See UdpSocketPosix for a
// reference implementation.
class UdpSocket {
 public:
  using Version = IPAddress::Version;

  // Creates a new, scoped UdpSocket within the IPv4 or IPv6 family.
  static UdpSocketUniquePtr Create(Version version);

  // Returns true if |socket| belongs to the IPv4/IPv6 address family.
  bool IsIPv4() const;
  bool IsIPv6() const;

  // Sets the socket for address reuse, binds to the address/port, and returns
  // true if the operations succeeded.
  bool Bind(const IPEndpoint& local_endpoint);

  // Sets the device to use for outgoing multicast packets on the socket, and
  // returns true if the operation succeeded.
  bool SetMulticastOutboundInterface(NetworkInterfaceIndex ifindex);

  // Joins to the multicast group at the given address, using the specified
  // interface, and returns true if the operation succeeded.
  bool JoinMulticastGroup(const IPAddress& address,
                          NetworkInterfaceIndex ifindex);

  // Performs a non-blocking read on the socket, returning the number of bytes
  // received (or absl::nullopt to indicate there was no message available or an
  // error occurred). Note that 0 is a valid result, indicating an empty message
  // has been received. |src| and |original_destination| are optional output
  // arguments that provide the source of the message and its intended
  // destination, respectively.
  absl::optional<size_t> ReceiveMessage(void* data,
                                        size_t length,
                                        IPEndpoint* src,
                                        IPEndpoint* original_destination);

  // Sends a message and returns the number of bytes sent on success. Otherwise,
  // absl::nullopt is returned to indicate the operation would block or an error
  // occurred.
  absl::optional<size_t> SendMessage(const void* data,
                                     size_t length,
                                     const IPEndpoint& dest);

 protected:
  UdpSocket();
  ~UdpSocket();

 private:
  DISALLOW_COPY_AND_ASSIGN(UdpSocket);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_SOCKET_H_
