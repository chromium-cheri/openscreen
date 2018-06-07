// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <ifaddrs.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <vector>

#include "base/ip_address.h"
#include "discovery/mdns/mdns_responder_platform.h"
#include "platform/api/error.h"
#include "platform/api/logging.h"
#include "platform/api/network_interface.h"
#include "platform/api/socket.h"
#include "platform/api/time.h"
#include "third_party/mDNSResponder/src/mDNSCore/mDNSEmbeddedAPI.h"

// man 3 netlink
// man 3 rtnetlink
// man 7 netlink
// man 7 rtnetlink
// man 7 ip
// man 7 netdevice

namespace {

std::vector<openscreen::platform::UdpSocketIPv4Ptr> SetupMulticastSocketsV4(
    const std::vector<int>& index_list) {
  std::vector<openscreen::platform::UdpSocketIPv4Ptr> fds;
  for (const auto ifindex : index_list) {
    auto* socket = openscreen::platform::CreateUdpSocketIPv4(
        openscreen::platform::BlockingType::kNonBlocking);
    if (!SetUdpMulticastPropertiesIPv4(socket, ifindex)) {
      LOG_ERROR << "multicast properties failed: "
                << openscreen::platform::GetLastErrorString();
      DestroyUdpSocket(socket);
      continue;
    }
    if (!JoinUdpMulticastGroupIPv4(
            socket, openscreen::IPv4Address{{224, 0, 0, 251}}, ifindex)) {
      LOG_ERROR << "join multicast group failed: "
                << openscreen::platform::GetLastErrorString();
      DestroyUdpSocket(socket);
      continue;
    }
    if (!BindUdpSocketIPv4(socket,
                           openscreen::IPv4Endpoint{
                               openscreen::IPv4Address{{0, 0, 0, 0}}, 5353})) {
      LOG_ERROR << "bind failed: "
                << openscreen::platform::GetLastErrorString();
      DestroyUdpSocket(socket);
      continue;
    }

    LOG_INFO << "listening on interface " << ifindex;
    fds.push_back(socket);
  }
  return fds;
}

// TODO
std::vector<openscreen::platform::UdpSocketIPv6Ptr> SetupMulticastSocketsV6(
    const std::vector<int>& index_list) {
  return {};
}

}  // namespace

extern "C" {

const char ProgramName[] = "openscreen";

mDNSs32 mDNSPlatformOneSecond = 1000;

mStatus mDNSPlatformInit(mDNS* m) {
  VLOG(2) << __func__;
  auto addrinfo = openscreen::platform::GetInterfaceAddresses();
  std::vector<int> v4_index_list;
  std::vector<int> v6_index_list;
  for (const auto& addr : addrinfo.v4_addresses) {
    v4_index_list.push_back(addr.info.index);
  };
  for (const auto& addr : addrinfo.v6_addresses) {
    v6_index_list.push_back(addr.info.index);
  };

  std::sort(begin(v4_index_list), end(v4_index_list));
  std::sort(begin(v6_index_list), end(v6_index_list));
  v4_index_list.erase(std::unique(begin(v4_index_list), end(v4_index_list)),
                      end(v4_index_list));
  v6_index_list.erase(std::remove_if(begin(v6_index_list), end(v6_index_list),
                                     [&v4_index_list](int index) {
                                       return std::find(begin(v4_index_list),
                                                        end(v4_index_list),
                                                        index) !=
                                              end(v4_index_list);
                                     }),
                      end(v6_index_list));
  m->p->v4_sockets = SetupMulticastSocketsV4(v4_index_list);
  m->p->v6_sockets = SetupMulticastSocketsV6(v6_index_list);
  // Listen on all interfaces
  // TODO: v6
  auto fd_it = begin(m->p->v4_sockets);
  for (int index : v4_index_list) {
    // Pick any address for the given interface.
    const auto& addr = *std::find_if(
        begin(addrinfo.v4_addresses), end(addrinfo.v4_addresses),
        [index](const openscreen::platform::IPv4InterfaceAddress& addr) {
          return addr.info.index == index;
        });
    NetworkInterfaceInfo* info = static_cast<NetworkInterfaceInfo*>(
        malloc(sizeof(NetworkInterfaceInfo)));
    std::memset(info, 0, sizeof(NetworkInterfaceInfo));
    info->InterfaceID = reinterpret_cast<decltype(info->InterfaceID)>(*fd_it++);
    info->ip.type = mDNSAddrType_IPv4;
    std::memcpy(info->ip.ip.v4.b, addr.address.bytes.data(),
                addr.address.bytes.size());
    info->mask.type = mDNSAddrType_IPv4;
    // XXX: little-endian-specific
    info->mask.ip.v4.NotAnInteger = 0xffffffffu >> (32 - addr.prefix_length);
    std::memcpy(info->MAC.b, addr.info.hardware_address,
                sizeof(addr.info.hardware_address));
    info->McastTxRx = 1;
    mDNS_RegisterInterface(m, info, mDNSfalse);
  }

  mDNSCoreInitComplete(m, mStatus_NoError);
  return mStatus_NoError;
}

void mDNSPlatformClose(mDNS* m) {
  VLOG(2) << __func__;
  for (auto* socket : m->p->v4_sockets) {
    openscreen::platform::DestroyUdpSocket(socket);
  }
  for (auto* socket : m->p->v6_sockets) {
    openscreen::platform::DestroyUdpSocket(socket);
  }
}

mStatus mDNSPlatformSendUDP(const mDNS* m,
                            const void* msg,
                            const mDNSu8* last,
                            mDNSInterfaceID InterfaceID,
                            UDPSocket* src,
                            const mDNSAddr* dst,
                            mDNSIPPort dstport) {
  VLOG(2) << __func__;
  const auto* ifv4 =
      reinterpret_cast<openscreen::platform::UdpSocketIPv4Ptr>(InterfaceID);
  // const auto* ifv6 = reinterpret_cast<UdpSocketIPv6Ptr>(InterfaceID);
  // TODO v6
  const auto fd_it =
      std::find(begin(m->p->v4_sockets), end(m->p->v4_sockets), ifv4);
  if (fd_it == end(m->p->v4_sockets)) {
    VLOG(2) << "bad interface";
    return mStatus_BadInterfaceErr;
  }

  openscreen::IPv4Endpoint dest{openscreen::IPv4Address{dst->ip.v4.b},
                                ntohs(dstport.NotAnInteger)};
  int64_t length = last - static_cast<const uint8_t*>(msg);
  openscreen::platform::SendUdpIPv4(*fd_it, msg, length, dest);
  return mStatus_NoError;
}

void mDNSPlatformLock(const mDNS* m) {
  VLOG(2) << __func__;
  // We're single threaded.
}

void mDNSPlatformUnlock(const mDNS* m) {
  VLOG(2) << __func__;
}

void mDNSPlatformStrCopy(void* dst, const void* src) {
  VLOG(2) << __func__;
  std::strcpy(static_cast<char*>(dst), static_cast<const char*>(src));
}

mDNSu32 mDNSPlatformStrLen(const void* src) {
  VLOG(2) << __func__;
  return std::strlen(static_cast<const char*>(src));
}

void mDNSPlatformMemCopy(void* dst, const void* src, mDNSu32 len) {
  VLOG(2) << __func__;
  std::memcpy(dst, src, len);
}

mDNSBool mDNSPlatformMemSame(const void* dst, const void* src, mDNSu32 len) {
  VLOG(2) << __func__;
  return std::memcmp(dst, src, len) == 0 ? mDNStrue : mDNSfalse;
}

void mDNSPlatformMemZero(void* dst, mDNSu32 len) {
  VLOG(2) << __func__;
  std::memset(dst, 0, len);
}

void* mDNSPlatformMemAllocate(mDNSu32 len) {
  VLOG(2) << __func__;
  return malloc(len);
}

void mDNSPlatformMemFree(void* mem) {
  VLOG(2) << __func__;
  free(mem);
}

mDNSu32 mDNSPlatformRandomSeed() {
  VLOG(2) << __func__;
  return std::chrono::steady_clock::now().time_since_epoch().count();
}

mStatus mDNSPlatformTimeInit() {
  VLOG(2) << __func__;
  return mStatus_NoError;
}

mDNSs32 mDNSPlatformRawTime() {
  VLOG(2) << __func__;
  return static_cast<int32_t>(
      ToMilliseconds(openscreen::platform::GetMonotonicTimeNow()).t);
}

mDNSs32 mDNSPlatformUTC() {
  VLOG(2) << __func__;
  return static_cast<int32_t>(
      ToMilliseconds(openscreen::platform::GetUTCNow()).t);
}

void mDNSPlatformWriteDebugMsg(const char* msg) {
  VLOG(2) << __func__ << ": " << msg;
}

void mDNSPlatformWriteLogMsg(const char* ident,
                             const char* msg,
                             mDNSLogLevel_t loglevel) {
  VLOG(2) << __func__ << ": " << msg;
}

TCPSocket* mDNSPlatformTCPSocket(mDNS* const m,
                                 TCPSocketFlags flags,
                                 mDNSIPPort* port) {
  VLOG(2) << __func__;
  return nullptr;
}

TCPSocket* mDNSPlatformTCPAccept(TCPSocketFlags flags, int sd) {
  VLOG(2) << __func__;
  return nullptr;
}

int mDNSPlatformTCPGetFD(TCPSocket* sock) {
  VLOG(2) << __func__;
  return 0;
}

mStatus mDNSPlatformTCPConnect(TCPSocket* sock,
                               const mDNSAddr* dst,
                               mDNSOpaque16 dstport,
                               domainname* hostname,
                               mDNSInterfaceID InterfaceID,
                               TCPConnectionCallback callback,
                               void* context) {
  VLOG(2) << __func__;
  return mStatus_NoError;
}

void mDNSPlatformTCPCloseConnection(TCPSocket* sock) {
  VLOG(2) << __func__;
}

long mDNSPlatformReadTCP(TCPSocket* sock,
                         void* buf,
                         unsigned long buflen,
                         mDNSBool* closed) {
  VLOG(2) << __func__;
  return 0;
}

long mDNSPlatformWriteTCP(TCPSocket* sock, const char* msg, unsigned long len) {
  VLOG(2) << __func__;
  return 0;
}

UDPSocket* mDNSPlatformUDPSocket(mDNS* const m,
                                 const mDNSIPPort requestedport) {
  VLOG(2) << __func__;
  return nullptr;
}

void mDNSPlatformUDPClose(UDPSocket* sock) {
  VLOG(2) << __func__;
}

void mDNSPlatformReceiveBPF_fd(mDNS* const m, int fd) {
  VLOG(2) << __func__;
}

void mDNSPlatformUpdateProxyList(mDNS* const m,
                                 const mDNSInterfaceID InterfaceID) {
  VLOG(2) << __func__;
}

void mDNSPlatformSendRawPacket(const void* const msg,
                               const mDNSu8* const end,
                               mDNSInterfaceID InterfaceID) {
  VLOG(2) << __func__;
}

void mDNSPlatformSetLocalAddressCacheEntry(mDNS* const m,
                                           const mDNSAddr* const tpa,
                                           const mDNSEthAddr* const tha,
                                           mDNSInterfaceID InterfaceID) {
  VLOG(2) << __func__;
}

void mDNSPlatformSourceAddrForDest(mDNSAddr* const src,
                                   const mDNSAddr* const dst) {
  VLOG(2) << __func__;
}

mStatus mDNSPlatformTLSSetupCerts(void) {
  VLOG(2) << __func__;
  return mStatus_NoError;
}

void mDNSPlatformTLSTearDownCerts(void) {
  VLOG(2) << __func__;
}

void mDNSPlatformSetDNSConfig(mDNS* const m,
                              mDNSBool setservers,
                              mDNSBool setsearch,
                              domainname* const fqdn,
                              DNameListElem** RegDomains,
                              DNameListElem** BrowseDomains) {
  VLOG(2) << __func__;
}

mStatus mDNSPlatformGetPrimaryInterface(mDNS* const m,
                                        mDNSAddr* v4,
                                        mDNSAddr* v6,
                                        mDNSAddr* router) {
  VLOG(2) << __func__;
  return mStatus_NoError;
}

void mDNSPlatformDynDNSHostNameStatusChanged(const domainname* const dname,
                                             const mStatus status) {
  VLOG(2) << __func__;
}

void mDNSPlatformSetAllowSleep(mDNS* const m,
                               mDNSBool allowSleep,
                               const char* reason) {
  VLOG(2) << __func__;
  VLOG(2) << allowSleep << " reason? " << reason;
}

void mDNSPlatformSendWakeupPacket(mDNS* const m,
                                  mDNSInterfaceID InterfaceID,
                                  char* EthAddr,
                                  char* IPAddr,
                                  int iteration) {
  VLOG(2) << __func__;
}

mDNSBool mDNSPlatformValidRecordForInterface(AuthRecord* rr,
                                             const NetworkInterfaceInfo* intf) {
  VLOG(2) << __func__;
  return mDNStrue;
}

}  // extern "C"
