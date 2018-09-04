// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_FAKE_MDNS_RESPONDER_ADAPTER_H_
#define API_IMPL_FAKE_MDNS_RESPONDER_ADAPTER_H_

#include <set>
#include <vector>

#include "discovery/mdns/mdns_responder_adapter.h"

namespace openscreen {

class FakeMdnsResponderAdapter;

mdns::PtrEvent MakePtrExample(const std::string& service_instance,
                              const std::string& service_type,
                              const std::string& service_protocol,
                              platform::UdpSocketIPv4Ptr socket);

mdns::SrvEvent MakeSrvExample(const std::string& service_instance,
                              const std::string& service_type,
                              const std::string& service_protocol,
                              const std::string& hostname,
                              uint16_t port,
                              platform::UdpSocketIPv4Ptr socket);

mdns::TxtEvent MakeTxtExample(const std::string& service_instance,
                              const std::string& service_type,
                              const std::string& service_protocol,
                              const std::vector<std::string>& txt_lines,
                              platform::UdpSocketIPv4Ptr socket);

mdns::AEvent MakeAExample(const std::string& hostname,
                          IPv4Address address,
                          platform::UdpSocketIPv4Ptr socket);

void AddEventsForNewService(FakeMdnsResponderAdapter* mdns_responder,
                            const std::string& service_instance,
                            const std::string& service_name,
                            const std::string& service_protocol,
                            const std::string& hostname,
                            uint16_t port,
                            const std::vector<std::string>& txt_lines,
                            const IPv4Address& address,
                            platform::UdpSocketIPv4Ptr socket);

// TODO(btolsch): Test this madness.
class FakeMdnsResponderAdapter final : public mdns::MdnsResponderAdapter {
 public:
  struct RegisteredInterface {
    platform::InterfaceInfo interface_info;
    platform::IPv4Subnet interface_address;
    platform::UdpSocketIPv4Ptr socket;
  };

  struct RegisteredService {
    std::string service_instance;
    std::string service_name;
    std::string service_protocol;
    mdns::DomainName target_host;
    uint16_t target_port;
    std::vector<std::string> lines;
  };

  virtual ~FakeMdnsResponderAdapter() override;

  void AddPtrEvent(mdns::PtrEvent&& ptr_event);
  void AddSrvEvent(mdns::SrvEvent&& srv_event);
  void AddTxtEvent(mdns::TxtEvent&& txt_event);
  void AddAEvent(mdns::AEvent&& a_event);

  const std::vector<RegisteredInterface>& registered_interfaces() {
    return registered_interfaces_;
  }
  const std::vector<RegisteredService>& registered_services() {
    return registered_services_;
  }
  bool running() const { return running_; }

  // mdns::MdnsResponderAdapter overrides.
  bool Init() override;
  void Close() override;

  bool SetHostLabel(const std::string& host_label) override;

  // TODO(btolsch): Reject/CHECK events that don't match any registered
  // interface?
  bool RegisterInterface(const platform::InterfaceInfo& interface_info,
                         const platform::IPv4Subnet& interface_address,
                         platform::UdpSocketIPv4Ptr socket) override;
  bool RegisterInterface(const platform::InterfaceInfo& interface_info,
                         const platform::IPv6Subnet& interface_address,
                         platform::UdpSocketIPv6Ptr socket) override;
  bool DeregisterInterface(platform::UdpSocketIPv4Ptr socket) override;
  bool DeregisterInterface(platform::UdpSocketIPv6Ptr socket) override;

  void OnDataReceived(const IPv4Endpoint& source,
                      const IPv4Endpoint& original_destination,
                      const uint8_t* data,
                      size_t length,
                      platform::UdpSocketIPv4Ptr receiving_socket) override;
  void OnDataReceived(const IPv6Endpoint& source,
                      const IPv6Endpoint& original_destination,
                      const uint8_t* data,
                      size_t length,
                      platform::UdpSocketIPv6Ptr receiving_socket) override;

  int RunTasks() override;

  std::vector<mdns::AEvent> TakeAResponses() override;
  std::vector<mdns::AaaaEvent> TakeAaaaResponses() override;
  std::vector<mdns::PtrEvent> TakePtrResponses() override;
  std::vector<mdns::SrvEvent> TakeSrvResponses() override;
  std::vector<mdns::TxtEvent> TakeTxtResponses() override;

  mdns::MdnsResponderErrorCode StartAQuery(
      const mdns::DomainName& domain_name) override;
  mdns::MdnsResponderErrorCode StartAaaaQuery(
      const mdns::DomainName& domain_name) override;
  mdns::MdnsResponderErrorCode StartPtrQuery(
      const mdns::DomainName& service_type) override;
  mdns::MdnsResponderErrorCode StartSrvQuery(
      const mdns::DomainName& service_instance) override;
  mdns::MdnsResponderErrorCode StartTxtQuery(
      const mdns::DomainName& service_instance) override;

  mdns::MdnsResponderErrorCode StopAQuery(
      const mdns::DomainName& domain_name) override;
  mdns::MdnsResponderErrorCode StopAaaaQuery(
      const mdns::DomainName& domain_name) override;
  mdns::MdnsResponderErrorCode StopPtrQuery(
      const mdns::DomainName& service_type) override;
  mdns::MdnsResponderErrorCode StopSrvQuery(
      const mdns::DomainName& service_instance) override;
  mdns::MdnsResponderErrorCode StopTxtQuery(
      const mdns::DomainName& service_instance) override;

  mdns::MdnsResponderErrorCode RegisterService(
      const std::string& service_instance,
      const std::string& service_name,
      const std::string& service_protocol,
      const mdns::DomainName& target_host,
      uint16_t target_port,
      const std::vector<std::string>& lines) override;
  mdns::MdnsResponderErrorCode DeregisterService(
      const std::string& service_instance,
      const std::string& service_name,
      const std::string& service_protocol) override;

 private:
  bool running_ = false;

  std::set<mdns::DomainName, mdns::DomainNameComparator> ptr_queries_;
  std::set<mdns::DomainName, mdns::DomainNameComparator> srv_queries_;
  std::set<mdns::DomainName, mdns::DomainNameComparator> txt_queries_;
  std::set<mdns::DomainName, mdns::DomainNameComparator> a_queries_;
  // NOTE: One of many simplifications here is that there is no cache.  This
  // means that calling StartQuery, StopQuery, StartQuery will only return an
  // event the first time, unless the test also adds the event a second time.
  std::vector<mdns::PtrEvent> ptr_events_;
  std::vector<mdns::SrvEvent> srv_events_;
  std::vector<mdns::TxtEvent> txt_events_;
  std::vector<mdns::AEvent> a_events_;

  std::vector<RegisteredInterface> registered_interfaces_;
  std::vector<RegisteredService> registered_services_;
};

}  // namespace openscreen

#endif  // API_IMPL_FAKE_MDNS_RESPONDER_ADAPTER_H_
