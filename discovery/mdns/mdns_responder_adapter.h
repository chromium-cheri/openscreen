// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_RESPONDER_ADAPTER_H_
#define DISCOVERY_MDNS_MDNS_RESPONDER_ADAPTER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/ip_address.h"
#include "discovery/mdns/domain_name.h"
#include "discovery/mdns/mdns_responder_platform.h"
#include "platform/api/network_interface.h"
#include "platform/api/socket.h"
#include "platform/base/event_loop.h"

namespace openscreen {
namespace mdns {

struct UdpSocketIPv4Description {
  IPv4Endpoint bind_endpoint;
  int32_t multicast_ifindex;
  IPv4Address multicast_address;
};

struct UdpSocketIPv6Description {
  IPv6Endpoint bind_endpoint;
  int32_t multicast_ifindex;
  IPv6Address multicast_address;
};

struct UdpSocketDescriptions {
  std::vector<UdpSocketIPv4Description> v4_descriptions;
  std::vector<UdpSocketIPv6Description> v6_descriptions;
};

enum class ResponseType {
  kAdd = 0,
  kAddNoCache,
  kRemove,
};

struct QueryResponseEventHeader {
  enum class SocketType {
    kIPv4 = 0,
    kIPv6,
  };

  QueryResponseEventHeader();
  QueryResponseEventHeader(ResponseType response_type,
                           platform::UdpSocketIPv4Ptr v4_socket);
  QueryResponseEventHeader(ResponseType response_type,
                           platform::UdpSocketIPv6Ptr v6_socket);
  QueryResponseEventHeader(const QueryResponseEventHeader&);
  ~QueryResponseEventHeader();
  QueryResponseEventHeader& operator=(const QueryResponseEventHeader&);

  ResponseType response_type;
  // Receiving socket.  Exactly one is set.
  SocketType which_socket;
  union {
    platform::UdpSocketIPv4Ptr v4_socket;
    platform::UdpSocketIPv6Ptr v6_socket;
  };
};

struct AResponseEvent {
  AResponseEvent();
  AResponseEvent(const QueryResponseEventHeader& header,
                 DomainName domain_name,
                 const IPv4Address& address);
  AResponseEvent(AResponseEvent&&);
  ~AResponseEvent();
  AResponseEvent& operator=(AResponseEvent&&);

  QueryResponseEventHeader header;
  DomainName domain_name;
  IPv4Address address;
};

struct AaaaResponseEvent {
  AaaaResponseEvent();
  AaaaResponseEvent(const QueryResponseEventHeader& header,
                    DomainName domain_name,
                    const IPv6Address& address);
  AaaaResponseEvent(AaaaResponseEvent&&);
  ~AaaaResponseEvent();
  AaaaResponseEvent& operator=(AaaaResponseEvent&&);

  QueryResponseEventHeader header;
  DomainName domain_name;
  IPv6Address address;
};

struct PtrResponseEvent {
  PtrResponseEvent();
  PtrResponseEvent(const QueryResponseEventHeader& header,
                   DomainName service_instance);
  PtrResponseEvent(PtrResponseEvent&&);
  ~PtrResponseEvent();
  PtrResponseEvent& operator=(PtrResponseEvent&&);

  QueryResponseEventHeader header;
  DomainName service_instance;
};

struct SrvResponseEvent {
  SrvResponseEvent();
  SrvResponseEvent(const QueryResponseEventHeader& header,
                   DomainName service_instance,
                   DomainName domain_name,
                   uint16_t port);
  SrvResponseEvent(SrvResponseEvent&&);
  ~SrvResponseEvent();
  SrvResponseEvent& operator=(SrvResponseEvent&&);

  QueryResponseEventHeader header;
  DomainName service_instance;
  DomainName domain_name;
  uint16_t port;
};

struct TxtResponseEvent {
  TxtResponseEvent();
  TxtResponseEvent(const QueryResponseEventHeader& header,
                   DomainName service_instance,
                   std::vector<std::string> txt_info);
  TxtResponseEvent(TxtResponseEvent&&);
  ~TxtResponseEvent();
  TxtResponseEvent& operator=(TxtResponseEvent&&);

  QueryResponseEventHeader header;
  DomainName service_instance;
  std::vector<std::string> txt_info;
};

enum class MdnsResponderError {
  kNoError = 0,
  kUnsupportedError,
  kDomainOverflowError,
  kUnknownError,
};

class MdnsResponderAdapter {
 public:
  static UdpSocketDescriptions GetUdpSocketRequests(
      const platform::InterfaceAddresses& interface_addresses);

  MdnsResponderAdapter();
  virtual ~MdnsResponderAdapter() = 0;

  virtual bool Init() = 0;
  virtual void Close() = 0;

  virtual void SetHostLabel(const std::string& host_label) = 0;

  // If RegisterInterface is called again to change |advertise| from false to
  // true, RegisterInterface will call DeregisterInterface first if necessary.
  virtual bool RegisterInterface(const platform::InterfaceInfo& interface_info,
                                 const platform::IPv4Subnet& interface_address,
                                 platform::UdpSocketIPv4Ptr socket,
                                 bool advertise) = 0;
  virtual bool RegisterInterface(const platform::InterfaceInfo& interface_info,
                                 const platform::IPv6Subnet& interface_address,
                                 platform::UdpSocketIPv6Ptr socket,
                                 bool advertise) = 0;
  virtual bool DeregisterInterface(platform::UdpSocketIPv4Ptr socket) = 0;
  virtual bool DeregisterInterface(platform::UdpSocketIPv6Ptr socket) = 0;

  virtual void OnDataReceived(const IPv4Endpoint& source,
                              const IPv4Endpoint& original_destination,
                              const uint8_t* data,
                              size_t length,
                              platform::UdpSocketIPv4Ptr receiving_socket) = 0;
  virtual void OnDataReceived(const IPv6Endpoint& source,
                              const IPv6Endpoint& original_destination,
                              const uint8_t* data,
                              size_t length,
                              platform::UdpSocketIPv6Ptr receiving_socket) = 0;

  // Returns the number of seconds after which this method must be called again.
  virtual int Execute() = 0;

  virtual std::vector<AResponseEvent> TakeAResponses() = 0;
  virtual std::vector<AaaaResponseEvent> TakeAaaaResponses() = 0;
  virtual std::vector<PtrResponseEvent> TakePtrResponses() = 0;
  virtual std::vector<SrvResponseEvent> TakeSrvResponses() = 0;
  virtual std::vector<TxtResponseEvent> TakeTxtResponses() = 0;

  virtual MdnsResponderError StartAQuery(const DomainName& domain_name) = 0;
  virtual MdnsResponderError StartAaaaQuery(const DomainName& domain_name) = 0;
  virtual MdnsResponderError StartPtrQuery(const DomainName& service_type) = 0;
  virtual MdnsResponderError StartSrvQuery(
      const DomainName& service_instance) = 0;
  virtual MdnsResponderError StartTxtQuery(
      const DomainName& service_instance) = 0;

  virtual MdnsResponderError StopAQuery(const DomainName& domain_name) = 0;
  virtual MdnsResponderError StopAaaaQuery(const DomainName& domain_name) = 0;
  virtual MdnsResponderError StopPtrQuery(const DomainName& service_type) = 0;
  virtual MdnsResponderError StopSrvQuery(
      const DomainName& service_instance) = 0;
  virtual MdnsResponderError StopTxtQuery(
      const DomainName& service_instance) = 0;

  virtual MdnsResponderError RegisterService(
      const std::string& service_name,
      const DomainName& service_type,
      const DomainName& target_host,
      uint16_t target_port,
      const std::vector<std::string>& lines) = 0;
};

}  // namespace mdns
}  // namespace openscreen

#endif
