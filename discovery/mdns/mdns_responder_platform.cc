// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_responder_platform.h"

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

#include <algorithm>
#include <chrono>
#include <cstring>
#include <vector>

#include "base/ip_address.h"
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

extern "C" {

const char ProgramName[] = "openscreen";

mDNSs32 mDNSPlatformOneSecond = 1000;

mStatus mDNSPlatformInit(mDNS* m) {
  VLOG(2) << __func__;
  mDNSCoreInitComplete(m, mStatus_NoError);
  return mStatus_NoError;
}

void mDNSPlatformClose(mDNS* m) {
  VLOG(2) << __func__;
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
  const auto fdv4_it =
      std::find(m->p->v4_sockets.begin(), m->p->v4_sockets.end(), ifv4);
  if (fdv4_it == m->p->v4_sockets.end()) {
    const auto* ifv6 =
        reinterpret_cast<openscreen::platform::UdpSocketIPv6Ptr>(InterfaceID);
    const auto fdv6_it =
        std::find(m->p->v6_sockets.begin(), m->p->v6_sockets.end(), ifv6);
    if (fdv6_it == m->p->v6_sockets.end()) {
      VLOG(2) << "bad interface";
      return mStatus_BadInterfaceErr;
    }
    openscreen::IPv6Endpoint dest{openscreen::IPv6Address{dst->ip.v6.b},
                                  ntohs(dstport.NotAnInteger)};
    int64_t length = last - static_cast<const uint8_t*>(msg);
    openscreen::platform::SendUdpIPv6(*fdv6_it, msg, length, dest);
    return mStatus_NoError;
  }

  openscreen::IPv4Endpoint dest{openscreen::IPv4Address{dst->ip.v4.b},
                                ntohs(dstport.NotAnInteger)};
  int64_t length = last - static_cast<const uint8_t*>(msg);
  openscreen::platform::SendUdpIPv4(*fdv4_it, msg, length, dest);
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
      openscreen::platform::GetMonotonicTimeNow().AsMilliseconds());
}

mDNSs32 mDNSPlatformUTC() {
  VLOG(2) << __func__;
  return static_cast<int32_t>(openscreen::platform::GetUTCNow().AsSeconds());
}

void mDNSPlatformWriteDebugMsg(const char* msg) {
  DVLOG(3) << __func__ << ": " << msg;
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
