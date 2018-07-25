// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/network_interface.h"

#include <linux/ethtool.h>
#include <linux/if_arp.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/sockios.h>
#include <linux/wireless.h>
#include <netinet/ip.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>

#include "base/ip_address.h"
#include "base/scoped_pipe.h"
#include "platform/api/logging.h"

namespace openscreen {
namespace platform {
namespace {

constexpr int kNetlinkRecvmsgBufSize = 8192;

std::string GetInterfaceName(const char* kernel_name) {
  std::string name;
  name.resize(IFNAMSIZ - 1);
  size_t name_len = strnlen(kernel_name, IFNAMSIZ);
  strncpy(&name[0], kernel_name, name_len);
  name.resize(name_len);
  return name;
}

InterfaceInfo::Type GetInterfaceType(const std::string& ifname) {
  // Determine type after name has been set.
  ScopedFd s(socket(AF_INET6, SOCK_DGRAM, 0));
  if (!s) {
    s = ScopedFd(socket(AF_INET, SOCK_DGRAM, 0));
    if (!s) {
      return InterfaceInfo::Type::kOther;
    }
  }

  // Note: This uses Wireless Extensions to test the interface, which is
  // deprecated.  However, it's much easier than using the new nl80211
  // interface for this purpose.  If Wireless Extensions are ever actually
  // removed though, this will need to use nl80211.
  struct iwreq wr;
  strncpy(wr.ifr_name, ifname.c_str(), IFNAMSIZ);
  if (ioctl(s.get(), SIOCGIWNAME, &wr) != -1) {
    return InterfaceInfo::Type::kWifi;
  }

  struct ethtool_cmd ecmd;
  ecmd.cmd = ETHTOOL_GSET;
  struct ifreq ifr;
  strncpy(ifr.ifr_name, ifname.c_str(), IFNAMSIZ);
  ifr.ifr_data = &ecmd;
  if (ioctl(s.get(), SIOCETHTOOL, &ifr) != -1) {
    return InterfaceInfo::Type::kEthernet;
  }

  return InterfaceInfo::Type::kOther;
}

void GetInterfaceAttributes(struct rtattr* rta,
                            unsigned int attrlen,
                            InterfaceInfo* info) {
  for (; RTA_OK(rta, attrlen); rta = RTA_NEXT(rta, attrlen)) {
    if (rta->rta_type == IFLA_IFNAME) {
      info->name =
          GetInterfaceName(reinterpret_cast<const char*>(RTA_DATA(rta)));
    } else if (rta->rta_type == IFLA_ADDRESS) {
      DCHECK_EQ(sizeof(info->hardware_address), RTA_PAYLOAD(rta));
      std::memcpy(info->hardware_address, RTA_DATA(rta),
                  sizeof(info->hardware_address));
    }
  }

  info->type = GetInterfaceType(info->name);
}

void GetIPv4Address(struct rtattr* rta,
                    unsigned int attrlen,
                    const std::string& ifname,
                    IPv4Address* address) {
  bool have_local = false;
  IPv4Address local;
  for (; RTA_OK(rta, attrlen); rta = RTA_NEXT(rta, attrlen)) {
    if (rta->rta_type == IFA_LABEL) {
      DCHECK_EQ(ifname, reinterpret_cast<const char*>(RTA_DATA(rta)));
    } else if (rta->rta_type == IFA_ADDRESS) {
      DCHECK_EQ(address->bytes.size(), RTA_PAYLOAD(rta));
      *address = IPv4Address(static_cast<uint8_t*>(RTA_DATA(rta)));
    } else if (rta->rta_type == IFA_LOCAL) {
      DCHECK_EQ(local.bytes.size(), RTA_PAYLOAD(rta));
      have_local = true;
      local = IPv4Address(static_cast<uint8_t*>(RTA_DATA(rta)));
    }
  }
  if (have_local) {
    *address = local;
  }
}

void GetIPv6Address(struct rtattr* rta,
                    unsigned int attrlen,
                    const std::string& ifname,
                    IPv6Address* address) {
  bool have_local = false;
  IPv6Address local;
  for (; RTA_OK(rta, attrlen); rta = RTA_NEXT(rta, attrlen)) {
    if (rta->rta_type == IFA_LABEL) {
      DCHECK_EQ(ifname, reinterpret_cast<const char*>(RTA_DATA(rta)));
    } else if (rta->rta_type == IFA_ADDRESS) {
      DCHECK_EQ(address->bytes.size(), RTA_PAYLOAD(rta));
      *address = IPv6Address(static_cast<uint8_t*>(RTA_DATA(rta)));
    } else if (rta->rta_type == IFA_LOCAL) {
      DCHECK_EQ(local.bytes.size(), RTA_PAYLOAD(rta));
      have_local = true;
      local = IPv6Address(static_cast<uint8_t*>(RTA_DATA(rta)));
    }
  }
  if (have_local) {
    *address = local;
  }
}

std::vector<InterfaceInfo> GetLinkInfo() {
  ScopedFd fd(socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE));
  if (!fd) {
    LOG_WARN << "netlink socket() failed: " << errno << " - "
             << strerror(errno);
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
  if (sendmsg(fd.get(), &msg, 0) < 0) {
    LOG_ERROR << "netlink sendmsg() failed: " << errno << " - "
              << strerror(errno);
    return {};
  }

  std::vector<InterfaceInfo> info_list;
  {
    char buf[kNetlinkRecvmsgBufSize];
    struct iovec iov = {buf, sizeof(buf)};
    struct sockaddr_nl sa;
    struct msghdr msg;
    struct nlmsghdr* nh;

    msg = {&sa, sizeof(sa), &iov, 1, nullptr, 0, 0};

    bool done = false;
    while (!done) {
      size_t len = recvmsg(fd.get(), &msg, 0);

      for (nh = reinterpret_cast<struct nlmsghdr*>(buf); NLMSG_OK(nh, len);
           nh = NLMSG_NEXT(nh, len)) {
        // The end of multipart message.
        if (nh->nlmsg_type == NLMSG_DONE) {
          done = true;
          break;
        } else if (nh->nlmsg_type == NLMSG_ERROR) {
          done = true;
          LOG_ERROR << "netlink error msg";
          continue;
        } else if ((nh->nlmsg_flags & NLM_F_MULTI) == 0) {
          done = true;
        }

        if (nh->nlmsg_type != RTM_NEWLINK) {
          continue;
        }

        struct ifinfomsg* ifi = static_cast<struct ifinfomsg*>(NLMSG_DATA(nh));
        if ((ifi->ifi_flags & IFF_LOOPBACK) ||
            ((ifi->ifi_flags & IFF_UP) == 0)) {
          continue;
        }
        info_list.emplace_back();
        InterfaceInfo& info = info_list.back();
        info.index = ifi->ifi_index;
        GetInterfaceAttributes(IFLA_RTA(ifi), IFLA_PAYLOAD(nh), &info);
      }
    }
  }

  return info_list;
}

std::vector<InterfaceAddresses> GetAddressInfo(
    const std::vector<InterfaceInfo>& info_list) {
  ScopedFd fd(socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE));
  if (!fd) {
    LOG_ERROR << "netlink socket() failed: " << errno << " - "
              << strerror(errno);
    return {};
  }

  // nl_pid = 0 to talk to the kernel.
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
  if (sendmsg(fd.get(), &msg, 0) < 0) {
    LOG_ERROR << "sendmsg failed: " << errno << " - " << strerror(errno);
    return {};
  }

  std::vector<InterfaceAddresses> address_list;
  {
    char buf[kNetlinkRecvmsgBufSize];
    struct iovec iov = {buf, sizeof(buf)};
    struct sockaddr_nl sa;
    struct msghdr msg;
    struct nlmsghdr* nh;

    msg = {&sa, sizeof(sa), &iov, 1, NULL, 0, 0};
    bool done = false;
    while (!done) {
      size_t len = recvmsg(fd.get(), &msg, 0);

      for (nh = reinterpret_cast<struct nlmsghdr*>(buf); NLMSG_OK(nh, len);
           nh = NLMSG_NEXT(nh, len)) {
        if (nh->nlmsg_type == NLMSG_DONE) {
          done = true;
          break;
        } else if (nh->nlmsg_type == NLMSG_ERROR) {
          done = true;
          LOG_ERROR << "netlink error msg";
          continue;
        } else if ((nh->nlmsg_flags & NLM_F_MULTI) == 0) {
          done = true;
        }

        if (nh->nlmsg_type != RTM_NEWADDR) {
          continue;
        }

        struct ifaddrmsg* ifa = static_cast<struct ifaddrmsg*>(NLMSG_DATA(nh));
        const auto info_it = std::find_if(
            info_list.begin(), info_list.end(),
            [ifa](const InterfaceInfo& info) {
              return info.index == static_cast<int32_t>(ifa->ifa_index);
            });
        if (info_it == info_list.end()) {
          VLOG(1) << "skipping address for interface " << ifa->ifa_index;
          continue;
        }
        auto addr_it =
            std::find_if(address_list.begin(), address_list.end(),
                         [ifa](const InterfaceAddresses& addresses) {
                           return addresses.info.index ==
                                  static_cast<int32_t>(ifa->ifa_index);
                         });
        if (addr_it == address_list.end()) {
          address_list.emplace_back();
          addr_it = address_list.end() - 1;
          addr_it->info = *info_it;
        }

        if (ifa->ifa_family == AF_INET) {
          addr_it->ipv4_addresses.emplace_back();
          IPv4Subnet& address = addr_it->ipv4_addresses.back();
          address.prefix_length = ifa->ifa_prefixlen;

          GetIPv4Address(IFA_RTA(ifa), IFA_PAYLOAD(nh), addr_it->info.name,
                         &address.address);
        } else if (ifa->ifa_family == AF_INET6) {
          addr_it->ipv6_addresses.emplace_back();
          IPv6Subnet& address = addr_it->ipv6_addresses.back();
          address.prefix_length = ifa->ifa_prefixlen;

          GetIPv6Address(IFA_RTA(ifa), IFA_PAYLOAD(nh), addr_it->info.name,
                         &address.address);
        } else {
          LOG_ERROR << "bad address family: " << ifa->ifa_family;
        }
      }
    }
  }

  return address_list;
}

}  // namespace

std::vector<InterfaceAddresses> GetInterfaceAddresses() {
  return GetAddressInfo(GetLinkInfo());
}

}  // namespace platform
}  // namespace openscreen
