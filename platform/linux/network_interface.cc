// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/network_interface.h"

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>

#include "base/ip_address.h"
#include "platform/api/logging.h"

namespace openscreen {
namespace platform {
namespace {

std::vector<InterfaceInfo> NetlinkLinkThings() {
  int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (fd == -1) {
    LOG_WARN << "netlink socket() failed";
    return {};
  }
  // pid = 0 for the kernel
  struct sockaddr_nl peer = {};
  peer.nl_family = AF_NETLINK;
  struct {
    struct nlmsghdr header;
    struct ifinfomsg msg;
  } request;

  request.header.nlmsg_len = sizeof(request);
  request.header.nlmsg_type = RTM_GETLINK;
  request.header.nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT;
  request.header.nlmsg_seq = 0;
  request.header.nlmsg_pid = 0;
  request.msg.ifi_family = AF_UNSPEC;
  struct iovec iov = {&request, request.header.nlmsg_len};
  struct msghdr msg = {&peer, sizeof(peer), &iov, 1, nullptr, 0, 0};
  if (sendmsg(fd, &msg, 0) < 0) {
    LOG_ERROR << "netlink sendmsg() failed";
    close(fd);
    return {};
  }

  std::vector<InterfaceInfo> info_list;
  {
    int len;
    char buf[8192];
    struct iovec iov = {buf, sizeof(buf)};
    struct sockaddr_nl sa;
    struct msghdr msg;
    struct nlmsghdr* nh;

    msg = {&sa, sizeof(sa), &iov, 1, nullptr, 0, 0};

    bool done = false;
    while (!done) {
      len = recvmsg(fd, &msg, 0);
      for (nh = (struct nlmsghdr*)buf; NLMSG_OK(nh, len);
           nh = NLMSG_NEXT(nh, len)) {
        /* The end of multipart message */
        if (nh->nlmsg_type == NLMSG_DONE) {
          done = true;
          break;
        }

        if (nh->nlmsg_type == NLMSG_ERROR)
          LOG_ERROR << "netlink error msg";
        struct ifinfomsg* ifi = static_cast<struct ifinfomsg*>(NLMSG_DATA(nh));
        if ((ifi->ifi_flags & IFF_LOOPBACK) ||
            ((ifi->ifi_flags & IFF_UP) == 0)) {
          continue;
        }
        info_list.emplace_back();
        auto& info = info_list.back();
        info.index = ifi->ifi_index;
        std::memset(info.hardware_address, 0, sizeof(info.hardware_address));
        unsigned int attrlen = IFLA_PAYLOAD(nh);
        for (struct rtattr* rta = IFLA_RTA(ifi); RTA_OK(rta, attrlen);
             rta = RTA_NEXT(rta, attrlen)) {
          if (rta->rta_type == IFLA_IFNAME) {
            info.name = reinterpret_cast<const char*>(RTA_DATA(rta));
          } else if (rta->rta_type == IFLA_ADDRESS) {
            DCHECK(RTA_PAYLOAD(rta) == sizeof(info.hardware_address));
            std::memcpy(info.hardware_address, RTA_DATA(rta),
                        sizeof(info.hardware_address));
          }
        }
      }
    }
  }
  close(fd);

  return info_list;
}

InterfaceAddresses NetlinkAddrThings(
    const std::vector<InterfaceInfo>& info_list) {
  int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (fd == -1) {
    LOG_ERROR << "netlink socket() failed";
    return {};
  }

  // nl_pid = 0 to talk to the kernel
  struct sockaddr_nl peer = {};
  peer.nl_family = AF_NETLINK;
  struct {
    struct nlmsghdr header;
    struct ifaddrmsg msg;
  } request;

  request.header.nlmsg_len = sizeof(request);
  request.header.nlmsg_type = RTM_GETADDR;
  request.header.nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT;
  request.header.nlmsg_seq = 1;
  request.header.nlmsg_pid = 0;
  request.msg.ifa_family = AF_UNSPEC;
  struct iovec iov = {&request, request.header.nlmsg_len};
  struct msghdr msg = {&peer, sizeof(peer), &iov, 1, nullptr, 0, 0};
  const int rv = sendmsg(fd, &msg, 0);
  if (rv < 0) {
    LOG_ERROR << "sendmsg failed";
    return {};
  }

  InterfaceAddresses address_list;
  {
    char buf[8192];
    struct iovec iov = {buf, sizeof(buf)};
    struct sockaddr_nl sa;
    struct msghdr msg;
    struct nlmsghdr* nh;

    msg = {&sa, sizeof(sa), &iov, 1, NULL, 0, 0};
    bool done = false;
    while (!done) {
      int len = recvmsg(fd, &msg, 0);
      LOG_INFO << "recv: " << len;

      for (nh = (struct nlmsghdr*)buf; NLMSG_OK(nh, len);
           nh = NLMSG_NEXT(nh, len)) {
        if (nh->nlmsg_type == NLMSG_DONE) {
          LOG_INFO << "done";
          done = true;
          continue;
        }

        if (nh->nlmsg_type == NLMSG_ERROR)
          LOG_ERROR << "netlink error msg";

        if (nh->nlmsg_type != RTM_NEWADDR) {
          continue;
        }

        struct ifaddrmsg* ifa = static_cast<struct ifaddrmsg*>(NLMSG_DATA(nh));
        const auto info_it = std::find_if(
            begin(info_list), end(info_list), [ifa](const InterfaceInfo& info) {
              return info.index == static_cast<int32_t>(ifa->ifa_index);
            });
        if (info_it == end(info_list)) {
          VLOG(1) << "skipping address for interface " << ifa->ifa_index;
          continue;
        }

        if (ifa->ifa_family == AF_INET) {
          address_list.v4_addresses.emplace_back();
          auto& address = address_list.v4_addresses.back();
          address.info = *info_it;
          address.prefix_length = ifa->ifa_prefixlen;

          unsigned int attrlen = IFA_PAYLOAD(nh);
          bool have_local = false;
          IPv4Address local;
          for (struct rtattr* rta = IFA_RTA(ifa); RTA_OK(rta, attrlen);
               rta = RTA_NEXT(rta, attrlen)) {
            if (rta->rta_type == IFA_LABEL) {
              DCHECK(address.info.name ==
                     reinterpret_cast<const char*>(RTA_DATA(rta)));
            } else if (rta->rta_type == IFA_ADDRESS) {
              address.address =
                  IPv4Address(static_cast<uint8_t*>(RTA_DATA(rta)));
            } else if (rta->rta_type == IFA_LOCAL) {
              have_local = true;
              local = IPv4Address(static_cast<uint8_t*>(RTA_DATA(rta)));
            }
          }
          if (have_local) {
            address.address = local;
          }
        } else if (ifa->ifa_family == AF_INET6) {
          LOG_WARN << "ipv6 unimplemented";
        } else {
          LOG_ERROR << "bad address family: " << ifa->ifa_family;
        }
      }
    }
  }
  close(fd);

  return address_list;
}

}  // namespace

InterfaceAddresses GetInterfaceAddresses() {
  return NetlinkAddrThings(NetlinkLinkThings());
}

}  // namespace platform
}  // namespace openscreen
