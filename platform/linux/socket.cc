// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/socket.h"

#include <linux/in6.h>
#include <linux/ipv6.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <memory>

#include "platform/api/logging.h"
#include "platform/linux/socket.h"

namespace openscreen {
namespace platform {

UdpSocketIPv4Ptr CreateUdpSocketIPv4(BlockingType blocking_type) {
  const int fd = socket(
      AF_INET,
      SOCK_DGRAM |
          (blocking_type == BlockingType::kNonBlocking ? SOCK_NONBLOCK : 0),
      0);
  if (fd == -1) {
    return nullptr;
  }
  return new UdpSocketIPv4Private{fd};
}

UdpSocketIPv6Ptr CreateUdpSocketIPv6(BlockingType blocking_type) {
  const int fd = socket(
      AF_INET6,
      SOCK_DGRAM |
          (blocking_type == BlockingType::kNonBlocking ? SOCK_NONBLOCK : 0),
      0);
  if (fd == -1) {
    return nullptr;
  }
  return new UdpSocketIPv6Private{fd};
}

void DestroyUdpSocket(UdpSocketIPv4Ptr socket) {
  DCHECK(socket);
  close(socket->fd);
  delete socket;
}

void DestroyUdpSocket(UdpSocketIPv6Ptr socket) {
  DCHECK(socket);
  close(socket->fd);
  delete socket;
}

bool BindUdpSocketIPv4(UdpSocketIPv4Ptr socket, const IPv4Endpoint& endpoint) {
  DCHECK(socket);
  DCHECK(socket->fd >= 0);
  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_port = htons(endpoint.port);
  address.sin_addr.s_addr = INADDR_ANY;
  if (bind(socket->fd, reinterpret_cast<struct sockaddr*>(&address),
           sizeof(address)) == -1) {
    return false;
  }
  return true;
}

bool BindUdpSocketIPv6(UdpSocketIPv6Ptr socket, const IPv6Endpoint& endpoint) {
  DCHECK(socket);
  DCHECK(socket->fd >= 0);
  struct sockaddr_in6 address;
  address.sin6_family = AF_INET6;
  address.sin6_flowinfo = 0;
  address.sin6_port = htons(endpoint.port);
  address.sin6_addr = IN6ADDR_ANY_INIT;
  address.sin6_scope_id = 0;
  if (bind(socket->fd, reinterpret_cast<struct sockaddr*>(&address),
           sizeof(address)) == -1) {
    return false;
  }
  return true;
}

bool SetUdpMulticastPropertiesIPv4(UdpSocketIPv4Ptr socket, int32_t ifindex) {
  DCHECK(socket);
  DCHECK(socket->fd >= 0);
  struct ip_mreqn mr;
  // Appropriate address is set based on |imr_ifindex| when set.
  mr.imr_address.s_addr = INADDR_ANY;
  mr.imr_multiaddr.s_addr = INADDR_ANY;
  mr.imr_ifindex = ifindex;
  if (setsockopt(socket->fd, IPPROTO_IP, IP_MULTICAST_IF, &mr, sizeof(mr)) ==
      -1) {
    return false;
  }
  const int reuse_addr = 1;
  if (setsockopt(socket->fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr,
                 sizeof(reuse_addr)) == -1) {
    return false;
  }
  return true;
}

bool SetUdpMulticastPropertiesIPv6(UdpSocketIPv6Ptr socket, int32_t ifindex) {
  DCHECK(socket);
  DCHECK(socket->fd >= 0);
  if (setsockopt(socket->fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifindex,
                 sizeof(ifindex)) == -1) {
    return false;
  }
  const int reuse_addr = 1;
  if (setsockopt(socket->fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr,
                 sizeof(reuse_addr)) == -1) {
    return false;
  }
  return true;
}

bool JoinUdpMulticastGroupIPv4(UdpSocketIPv4Ptr socket,
                               const IPv4Address& address,
                               int32_t ifindex) {
  DCHECK(socket);
  DCHECK(socket->fd >= 0);
  const int on = 1;
  if (setsockopt(socket->fd, IPPROTO_IP, IP_PKTINFO, &on, sizeof(on)) == -1) {
    return false;
  }
  struct ip_mreqn mr;
  // Appropriate address is set based on |imr_ifindex| when set.
  mr.imr_address.s_addr = INADDR_ANY;
  mr.imr_ifindex = ifindex;
  std::memcpy(&mr.imr_multiaddr, address.bytes.data(),
              sizeof(mr.imr_multiaddr));
  if (setsockopt(socket->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mr, sizeof(mr)) ==
      -1) {
    return false;
  }
  return true;
}

bool JoinUdpMulticastGroupIPv6(UdpSocketIPv6Ptr socket,
                               const IPv6Address& address,
                               int32_t ifindex) {
  DCHECK(socket);
  DCHECK(socket->fd >= 0);
  const int on = 1;
  if (setsockopt(socket->fd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on)) ==
      -1) {
    return false;
  }
  struct ipv6_mreq mr;
  mr.ipv6mr_ifindex = ifindex;
  std::memcpy(&mr.ipv6mr_multiaddr, address.bytes.data(),
              sizeof(mr.ipv6mr_multiaddr));
  if (setsockopt(socket->fd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mr,
                 sizeof(mr)) == -1) {
    return false;
  }
  return true;
}

int64_t ReceiveUdpIPv4(UdpSocketIPv4Ptr socket,
                       void* data,
                       int64_t length,
                       IPv4Endpoint* src,
                       IPv4Endpoint* original_destination) {
  DCHECK(socket);
  DCHECK(socket->fd >= 0);
  DCHECK(src);
  struct iovec iov = {data, static_cast<size_t>(length)};
  struct sockaddr_in sa;
  char control_buf[1024];
  struct msghdr msg;
  msg.msg_name = &sa;
  msg.msg_namelen = sizeof(sa);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = control_buf;
  msg.msg_controllen = sizeof(control_buf);
  msg.msg_flags = 0;

  int64_t len = recvmsg(socket->fd, &msg, 0);
  if (len < 0) {
    return len;
  }
  std::memcpy(src->address.bytes.data(), &sa.sin_addr.s_addr,
              sizeof(sa.sin_addr.s_addr));
  src->port = ntohs(sa.sin_port);

  if (original_destination) {
    std::memset(original_destination, 0, sizeof(*original_destination));
    for (struct cmsghdr* cmh = CMSG_FIRSTHDR(&msg); cmh;
         cmh = CMSG_NXTHDR(&msg, cmh)) {
      if (cmh->cmsg_level != IPPROTO_IP || cmh->cmsg_type != IP_PKTINFO) {
        continue;
      }
      struct in_pktinfo* pktinfo =
          reinterpret_cast<struct in_pktinfo*>(CMSG_DATA(cmh));
      std::memcpy(original_destination->address.bytes.data(),
                  &pktinfo->ipi_addr, sizeof(pktinfo->ipi_addr));
      original_destination->port = 5353;
      break;
    }
  }

  return len;
}

int64_t ReceiveUdpIPv6(UdpSocketIPv6Ptr socket,
                       void* data,
                       int64_t length,
                       IPv6Endpoint* src,
                       IPv6Endpoint* original_destination) {
  DCHECK(socket);
  DCHECK(socket->fd >= 0);
  DCHECK(src);
  struct iovec iov = {data, static_cast<size_t>(length)};
  struct sockaddr_in6 sa;
  char control_buf[1024];
  struct msghdr msg;
  msg.msg_name = &sa;
  msg.msg_namelen = sizeof(sa);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = control_buf;
  msg.msg_controllen = sizeof(control_buf);
  msg.msg_flags = 0;

  int64_t len = recvmsg(socket->fd, &msg, 0);
  if (len < 0) {
    return len;
  }
  std::memcpy(src->address.bytes.data(), &sa.sin6_addr.s6_addr,
              sizeof(sa.sin6_addr.s6_addr));
  src->port = ntohs(sa.sin6_port);

  if (original_destination) {
    std::memset(original_destination, 0, sizeof(*original_destination));
    for (struct cmsghdr* cmh = CMSG_FIRSTHDR(&msg); cmh;
         cmh = CMSG_NXTHDR(&msg, cmh)) {
      if (cmh->cmsg_level != IPPROTO_IPV6 || cmh->cmsg_type != IPV6_PKTINFO) {
        continue;
      }
      struct in6_pktinfo* pktinfo =
          reinterpret_cast<struct in6_pktinfo*>(CMSG_DATA(cmh));
      std::memcpy(original_destination->address.bytes.data(),
                  &pktinfo->ipi6_addr, sizeof(pktinfo->ipi6_addr));
      original_destination->port = 5353;
      break;
    }
  }

  return len;
}

int64_t SendUdpIPv4(UdpSocketIPv4Ptr socket,
                    const void* data,
                    int64_t length,
                    const IPv4Endpoint& dest) {
  DCHECK(socket);
  DCHECK(socket->fd >= 0);
  struct sockaddr_in sa = {
      .sin_family = AF_INET,
      .sin_port = htons(dest.port),
  };
  std::memcpy(&sa.sin_addr.s_addr, dest.address.bytes.data(),
              sizeof(sa.sin_addr.s_addr));
  struct iovec iov = {const_cast<void*>(data), static_cast<size_t>(length)};
  struct msghdr msg;
  msg.msg_name = &sa;
  msg.msg_namelen = sizeof(sa);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = nullptr;
  msg.msg_controllen = 0;
  msg.msg_flags = 0;
  return sendmsg(socket->fd, &msg, 0);
}

int64_t SendUdpIPv6(UdpSocketIPv6Ptr socket,
                    const void* data,
                    int64_t length,
                    const IPv6Endpoint& dest) {
  DCHECK(socket);
  DCHECK(socket->fd >= 0);
  struct sockaddr_in6 sa = {
      .sin6_family = AF_INET6,
      .sin6_port = htons(dest.port),
  };
  std::memcpy(&sa.sin6_addr.s6_addr, dest.address.bytes.data(),
              sizeof(sa.sin6_addr.s6_addr));
  struct iovec iov = {const_cast<void*>(data), static_cast<size_t>(length)};
  struct msghdr msg;
  msg.msg_name = &sa;
  msg.msg_namelen = sizeof(sa);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = nullptr;
  msg.msg_controllen = 0;
  msg.msg_flags = 0;
  return sendmsg(socket->fd, &msg, 0);
}

}  // namespace platform
}  // namespace openscreen
