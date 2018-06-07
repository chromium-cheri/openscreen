// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_responder_adapter.h"

// XXX for htons
#include <arpa/inet.h>

#include <cstring>
#include <iostream>

#include "platform/api/logging.h"

namespace openscreen {
namespace mdns {
namespace {

void MdnsStatusCallback(mDNS* mdns, mStatus result) {
  LOG_INFO << "status good? " << (result == mStatus_NoError);
}

void BrowseCallback(mDNS* m,
                    DNSQuestion* question,
                    const ResourceRecord* answer,
                    QC_result add_record) {
  LOG_INFO << __func__;
  char name_cstr[MAX_DOMAIN_LABEL + 1];
  char type_cstr[MAX_ESCAPED_DOMAIN_NAME + 1];
  char domain_cstr[MAX_ESCAPED_DOMAIN_NAME + 1];
  domainlabel name;
  domainname type;
  domainname domain;
  DeconstructServiceName(&answer->rdata->u.name, &name, &type, &domain);
  ConvertDomainLabelToCString_unescaped(&name, name_cstr);
  ConvertDomainNameToCString(&type, type_cstr);
  ConvertDomainNameToCString(&domain, domain_cstr);
  LOG_INFO << ((add_record == QC_rmv) ? "Lost" : "Found") << ": (" << name_cstr
           << ", " << type_cstr << ", " << domain_cstr << ")";
}

}  // namespace

bool MdnsResponderAdapter::Init() {
  VLOG(2) << __func__;
  const auto err =
      mDNS_Init(&mdns_, &platform_storage_, rr_cache_, kRrCacheSize,
                mDNS_Init_DontAdvertiseLocalAddresses, &MdnsStatusCallback,
                mDNS_Init_NoInitCallbackContext);
  LOG_INFO << "init good? " << (err == mStatus_NoError);
  return err == mStatus_NoError;
}

void MdnsResponderAdapter::Close() {
  mDNS_Close(&mdns_);
}

void MdnsResponderAdapter::OnDataReceived(
    const IPv4Endpoint& source,
    const IPv4Endpoint& original_destination,
    const uint8_t* data,
    int32_t length,
    platform::UdpSocketIPv4Ptr receiving_socket) {
  VLOG(2) << __func__;
  mDNSAddr src;
  src.type = mDNSAddrType_IPv4;
  std::memcpy(src.ip.v4.b, source.address.bytes.data(), sizeof(src.ip.v4.b));
  mDNSIPPort srcport;
  srcport.NotAnInteger = htons(source.port);

  mDNSAddr dst;
  dst.type = mDNSAddrType_IPv4;
  std::memcpy(dst.ip.v4.b, original_destination.address.bytes.data(),
              sizeof(dst.ip.v4.b));
  mDNSIPPort dstport;
  dstport.NotAnInteger = htons(original_destination.port);

  mDNSCoreReceive(&mdns_, const_cast<uint8_t*>(data), data + length, &src,
                  srcport, &dst, dstport,
                  reinterpret_cast<mDNSInterfaceID>(receiving_socket));
}
void MdnsResponderAdapter::OnDataReceived(
    const IPv6Endpoint& source,
    const IPv6Endpoint& original_destination,
    const uint8_t* data,
    int32_t length,
    platform::UdpSocketIPv6Ptr receiving_socket) {
  VLOG(2) << __func__;
  mDNSAddr src;
  src.type = mDNSAddrType_IPv6;
  std::memcpy(src.ip.v6.b, source.address.bytes.data(), sizeof(src.ip.v6.b));
  mDNSIPPort srcport;
  srcport.NotAnInteger = htons(source.port);

  mDNSAddr dst;
  dst.type = mDNSAddrType_IPv6;
  std::memcpy(dst.ip.v6.b, original_destination.address.bytes.data(),
              sizeof(dst.ip.v6.b));
  mDNSIPPort dstport;
  dstport.NotAnInteger = htons(original_destination.port);

  mDNSCoreReceive(&mdns_, const_cast<uint8_t*>(data), data + length, &src,
                  srcport, &dst, dstport,
                  reinterpret_cast<mDNSInterfaceID>(receiving_socket));
}

void MdnsResponderAdapter::Execute() {
  VLOG(2) << __func__;
  // TODO: return time but try to handle 64->32->64 overflow
  const auto t = mDNS_Execute(&mdns_);
  const auto now = mDNSPlatformRawTime();
  LOG_INFO << "next execute: " << t << ", " << now << ", " << (t - now);
}

bool MdnsResponderAdapter::StartBrowse(const std::string& service_type) {
  VLOG(2) << __func__;
  domainname type;
  domainname domain;
  MakeDomainNameFromDNSNameString(&type, service_type.data());
  MakeDomainNameFromDNSNameString(&domain, "local.");
  question_.QuestionContext = this;
  const auto err =
      mDNS_StartBrowse(&mdns_, &question_, &type, &domain, mDNSInterface_Any,
                       mDNSfalse, &BrowseCallback, this);
  LOG_IF(WARN, err != mStatus_NoError) << "mDNSStartBrowse failed: " << err;
  return err == mStatus_NoError;
}

bool MdnsResponderAdapter::StopBrowse(const std::string& service_type) {
  VLOG(2) << __func__;
  const auto err = mDNS_StopBrowse(&mdns_, &question_);
  return err == mStatus_NoError;
}

const std::vector<platform::UdpSocketIPv4Ptr>&
MdnsResponderAdapter::GetIPv4SocketsToWatch() {
  return platform_storage_.v4_sockets;
}

const std::vector<platform::UdpSocketIPv6Ptr>&
MdnsResponderAdapter::GetIPv6SocketsToWatch() {
  return platform_storage_.v6_sockets;
}

}  // namespace mdns
}  // namespace openscreen
