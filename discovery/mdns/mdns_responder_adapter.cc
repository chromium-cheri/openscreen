// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_responder_adapter.h"

namespace openscreen {
namespace mdns {

QueryResponseEventHeader::QueryResponseEventHeader() = default;
QueryResponseEventHeader::QueryResponseEventHeader(
    ResponseType response_type,
    platform::UdpSocketIPv4Ptr v4_socket)
    : response_type(response_type),
      which_socket(SocketType::kIPv4),
      v4_socket(v4_socket) {}
QueryResponseEventHeader::QueryResponseEventHeader(
    ResponseType response_type,
    platform::UdpSocketIPv6Ptr v6_socket)
    : response_type(response_type),
      which_socket(SocketType::kIPv6),
      v6_socket(v6_socket) {}
QueryResponseEventHeader::QueryResponseEventHeader(
    const QueryResponseEventHeader&) = default;
QueryResponseEventHeader::~QueryResponseEventHeader() = default;
QueryResponseEventHeader& QueryResponseEventHeader::operator=(
    const QueryResponseEventHeader&) = default;

AResponseEvent::AResponseEvent() = default;
AResponseEvent::AResponseEvent(const QueryResponseEventHeader& header,
                               DomainName domain_name,
                               const IPv4Address& address)
    : header(header), domain_name(std::move(domain_name)), address(address) {}
AResponseEvent::AResponseEvent(AResponseEvent&&) = default;
AResponseEvent::~AResponseEvent() = default;
AResponseEvent& AResponseEvent::operator=(AResponseEvent&&) = default;

AaaaResponseEvent::AaaaResponseEvent() = default;
AaaaResponseEvent::AaaaResponseEvent(const QueryResponseEventHeader& header,
                                     DomainName domain_name,
                                     const IPv6Address& address)
    : header(header), domain_name(std::move(domain_name)), address(address) {}
AaaaResponseEvent::AaaaResponseEvent(AaaaResponseEvent&&) = default;
AaaaResponseEvent::~AaaaResponseEvent() = default;
AaaaResponseEvent& AaaaResponseEvent::operator=(AaaaResponseEvent&&) = default;

PtrResponseEvent::PtrResponseEvent() = default;
PtrResponseEvent::PtrResponseEvent(const QueryResponseEventHeader& header,
                                   DomainName service_instance)
    : header(header), service_instance(std::move(service_instance)) {}
PtrResponseEvent::PtrResponseEvent(PtrResponseEvent&&) = default;
PtrResponseEvent::~PtrResponseEvent() = default;
PtrResponseEvent& PtrResponseEvent::operator=(PtrResponseEvent&&) = default;

SrvResponseEvent::SrvResponseEvent() = default;
SrvResponseEvent::SrvResponseEvent(const QueryResponseEventHeader& header,
                                   DomainName service_instance,
                                   DomainName domain_name,
                                   uint16_t port)
    : header(header),
      service_instance(std::move(service_instance)),
      domain_name(std::move(domain_name)),
      port(port) {}
SrvResponseEvent::SrvResponseEvent(SrvResponseEvent&&) = default;
SrvResponseEvent::~SrvResponseEvent() = default;
SrvResponseEvent& SrvResponseEvent::operator=(SrvResponseEvent&&) = default;

TxtResponseEvent::TxtResponseEvent() = default;
TxtResponseEvent::TxtResponseEvent(const QueryResponseEventHeader& header,
                                   DomainName service_instance,
                                   std::vector<std::string> txt_info)
    : header(header),
      service_instance(std::move(service_instance)),
      txt_info(std::move(txt_info)) {}
TxtResponseEvent::TxtResponseEvent(TxtResponseEvent&&) = default;
TxtResponseEvent::~TxtResponseEvent() = default;
TxtResponseEvent& TxtResponseEvent::operator=(TxtResponseEvent&&) = default;

MdnsResponderAdapter::MdnsResponderAdapter() = default;
MdnsResponderAdapter::~MdnsResponderAdapter() = default;

}  // namespace mdns
}  // namespace openscreen
