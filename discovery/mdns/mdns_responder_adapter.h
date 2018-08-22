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

struct QueryEventHeader {
  enum class Type {
    kAdded = 0,
    kAddedNoCache,
    kRemoved,
  };

  enum class SocketType {
    kIPv4 = 0,
    kIPv6,
  };

  QueryEventHeader();
  QueryEventHeader(Type response_type, platform::UdpSocketIPv4Ptr v4_socket);
  QueryEventHeader(Type response_type, platform::UdpSocketIPv6Ptr v6_socket);
  QueryEventHeader(const QueryEventHeader&);
  ~QueryEventHeader();
  QueryEventHeader& operator=(const QueryEventHeader&);

  Type response_type;
  // Receiving socket.  Exactly one is set.
  SocketType receiving_socket_type;
  union {
    platform::UdpSocketIPv4Ptr v4_socket;
    platform::UdpSocketIPv6Ptr v6_socket;
  };
};

struct AEvent {
  AEvent();
  AEvent(const QueryEventHeader& header,
         DomainName domain_name,
         const IPv4Address& address);
  AEvent(AEvent&&);
  ~AEvent();
  AEvent& operator=(AEvent&&);

  QueryEventHeader header;
  DomainName domain_name;
  IPv4Address address;
};

struct AaaaEvent {
  AaaaEvent();
  AaaaEvent(const QueryEventHeader& header,
            DomainName domain_name,
            const IPv6Address& address);
  AaaaEvent(AaaaEvent&&);
  ~AaaaEvent();
  AaaaEvent& operator=(AaaaEvent&&);

  QueryEventHeader header;
  DomainName domain_name;
  IPv6Address address;
};

struct PtrEvent {
  PtrEvent();
  PtrEvent(const QueryEventHeader& header, DomainName service_instance);
  PtrEvent(PtrEvent&&);
  ~PtrEvent();
  PtrEvent& operator=(PtrEvent&&);

  QueryEventHeader header;
  DomainName service_instance;
};

struct SrvEvent {
  SrvEvent();
  SrvEvent(const QueryEventHeader& header,
           DomainName service_instance,
           DomainName domain_name,
           uint16_t port);
  SrvEvent(SrvEvent&&);
  ~SrvEvent();
  SrvEvent& operator=(SrvEvent&&);

  QueryEventHeader header;
  DomainName service_instance;
  DomainName domain_name;
  uint16_t port;
};

struct TxtEvent {
  TxtEvent();
  TxtEvent(const QueryEventHeader& header,
           DomainName service_instance,
           std::vector<std::string> txt_info);
  TxtEvent(TxtEvent&&);
  ~TxtEvent();
  TxtEvent& operator=(TxtEvent&&);

  QueryEventHeader header;
  DomainName service_instance;
  std::vector<std::string> txt_info;
};

enum class MdnsResponderErrorCode {
  kNoError = 0,
  kUnsupportedError,
  kDomainOverflowError,
  kUnknownError,
};

class MdnsResponderAdapter {
 public:
  MdnsResponderAdapter();
  virtual ~MdnsResponderAdapter() = 0;

  // Initializes mDNSResponder.  This should be called before any queries or
  // service registrations are made.
  virtual bool Init() = 0;

  // Stops all open queries and service registrations.  If this is not called
  // before destruction, any registered services will not send their goodbye
  // messages.
  virtual void Close() = 0;

  // Called to change the name published by the A and AAAA records for the host
  // when any service is active (via RegisterService).
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

  virtual std::vector<AEvent> TakeAResponses() = 0;
  virtual std::vector<AaaaEvent> TakeAaaaResponses() = 0;
  virtual std::vector<PtrEvent> TakePtrResponses() = 0;
  virtual std::vector<SrvEvent> TakeSrvResponses() = 0;
  virtual std::vector<TxtEvent> TakeTxtResponses() = 0;

  virtual MdnsResponderErrorCode StartAQuery(const DomainName& domain_name) = 0;
  virtual MdnsResponderErrorCode StartAaaaQuery(
      const DomainName& domain_name) = 0;
  virtual MdnsResponderErrorCode StartPtrQuery(
      const DomainName& service_type) = 0;
  virtual MdnsResponderErrorCode StartSrvQuery(
      const DomainName& service_instance) = 0;
  virtual MdnsResponderErrorCode StartTxtQuery(
      const DomainName& service_instance) = 0;

  virtual MdnsResponderErrorCode StopAQuery(const DomainName& domain_name) = 0;
  virtual MdnsResponderErrorCode StopAaaaQuery(
      const DomainName& domain_name) = 0;
  virtual MdnsResponderErrorCode StopPtrQuery(
      const DomainName& service_type) = 0;
  virtual MdnsResponderErrorCode StopSrvQuery(
      const DomainName& service_instance) = 0;
  virtual MdnsResponderErrorCode StopTxtQuery(
      const DomainName& service_instance) = 0;

  // TODO(btolsch): Need to be able to unregister a service outside of just
  // calling Close.
  virtual MdnsResponderErrorCode RegisterService(
      const std::string& service_name,
      const DomainName& service_type,
      const DomainName& target_host,
      uint16_t target_port,
      const std::vector<std::string>& lines) = 0;
};

}  // namespace mdns
}  // namespace openscreen

#endif
