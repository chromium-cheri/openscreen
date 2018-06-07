// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_RESPONDER_ADAPTER_H_
#define DISCOVERY_MDNS_MDNS_RESPONDER_ADAPTER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/ip_address.h"
#include "discovery/mdns/mdns_responder_platform.h"
#include "platform/api/socket.h"
#include "platform/base/event_loop.h"
#include "third_party/mDNSResponder/src/mDNSCore/mDNSEmbeddedAPI.h"

namespace openscreen {
namespace mdns {

class MdnsResponderAdapter {
 public:
  static constexpr int kRrCacheSize = 500;

  MdnsResponderAdapter() = default;
  virtual ~MdnsResponderAdapter() = default;

  virtual bool Init();
  virtual void Close();

  virtual void OnDataReceived(const IPv4Endpoint& source,
                              const IPv4Endpoint& original_destination,
                              const uint8_t* data,
                              int32_t length,
                              platform::UdpSocketIPv4Ptr receiving_socket);
  virtual void OnDataReceived(const IPv6Endpoint& source,
                              const IPv6Endpoint& original_destination,
                              const uint8_t* data,
                              int32_t length,
                              platform::UdpSocketIPv6Ptr receiving_socket);
  // TODO: Execute() scheduling requirements
  virtual void Execute();

  // TODO: Map mDNSResponder error codes to new enum instead of just bool.
  virtual bool StartBrowse(const std::string& service_type);
  virtual bool StopBrowse(const std::string& service_type);

  const std::vector<platform::UdpSocketIPv4Ptr>& GetIPv4SocketsToWatch();
  const std::vector<platform::UdpSocketIPv6Ptr>& GetIPv6SocketsToWatch();

 private:
  CacheEntity rr_cache_[kRrCacheSize];
  mDNS mdns_;
  mDNS_PlatformSupport platform_storage_;
  // TODO: std::map of questions
  DNSQuestion question_;
};

}  // namespace mdns
}  // namespace openscreen

#endif
