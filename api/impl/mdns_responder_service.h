// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_MDNS_RESPONDER_SERVICE_H_
#define API_IMPL_MDNS_RESPONDER_SERVICE_H_

#include <array>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "api/impl/mdns_platform_service.h"
#include "api/impl/screen_listener_impl.h"
#include "api/impl/screen_publisher_impl.h"
#include "base/ip_address.h"
#include "discovery/mdns/mdns_responder_adapter.h"
#include "platform/api/network_interface.h"
#include "platform/base/event_loop.h"

namespace openscreen {

class MdnsResponderAdapterFactory {
 public:
  virtual ~MdnsResponderAdapterFactory() = default;

  virtual std::unique_ptr<mdns::MdnsResponderAdapter> Create() = 0;
};

class MdnsResponderService final : public ScreenListenerImpl::Delegate,
                                   public ScreenPublisherImpl::Delegate {
 public:
  explicit MdnsResponderService(
      const std::string& service_name,
      const std::string& service_protocol,
      std::unique_ptr<MdnsResponderAdapterFactory> mdns_responder_factory,
      std::unique_ptr<MdnsPlatformService> platform);
  ~MdnsResponderService() override;

  void SetServiceConfig(const std::string& hostname,
                        const std::string& instance,
                        uint16_t port,
                        const std::vector<platform::InterfaceIndex> whitelist,
                        const std::map<std::string, std::string>& txt_data);

  void HandleNewEvents(const std::vector<platform::ReceivedData>& data);

  // ScreenListenerImpl::Delegate overrides.
  void StartListener() override;
  void StartAndSuspendListener() override;
  void StopListener() override;
  void SuspendListener() override;
  void ResumeListener() override;
  void SearchNow(ScreenListener::State from) override;

  // ScreenPublisherImpl::Delegate overrides.
  void StartPublisher() override;
  void StartAndSuspendPublisher() override;
  void StopPublisher() override;
  void SuspendPublisher() override;
  void ResumePublisher() override;

 private:
  // NOTE: service_instance implicit in map key.
  struct ServiceInstance {
    platform::UdpSocketPtr ptr_socket = nullptr;
    mdns::DomainName domain_name;
    uint16_t port = 0;
    bool has_ptr_record = false;
    std::vector<std::string> txt_info;

    // |port| == 0 signals that we have no SRV record.
    bool has_srv() const { return port != 0; }
  };

  // NOTE: hostname implicit in map key.
  struct HostInfo {
    std::vector<ServiceInstance*> services;
    IPAddress v4_address;
    IPAddress v6_address;
  };

  using PendingScreenInfoSet =
      std::set<mdns::DomainName, mdns::DomainNameComparator>;

  void HandleMdnsEvents();
  void StartListening();
  void StopListening();
  void StartService();
  void StopService();
  void StopMdnsResponder();
  void UpdatePendingScreenInfoSet(PendingScreenInfoSet* pending_screen_info_set,
                                  const mdns::DomainName& domain_name);
  void RemoveAllScreens();

  bool HandlePtrEvent(const mdns::PtrEvent& ptr_event,
                      PendingScreenInfoSet* pending_screen_info_set);
  bool HandleSrvEvent(const mdns::SrvEvent& srv_event,
                      PendingScreenInfoSet* pending_screen_info_set);
  bool HandleTxtEvent(const mdns::TxtEvent& txt_event,
                      PendingScreenInfoSet* pending_screen_info_set);
  bool HandleAddressEvent(platform::UdpSocketPtr socket,
                          mdns::QueryEventHeader::Type response_type,
                          const mdns::DomainName& domain_name,
                          bool a_event,
                          const IPAddress& address,
                          PendingScreenInfoSet* pending_screen_info_set);
  bool HandleAEvent(const mdns::AEvent& a_event,
                    PendingScreenInfoSet* pending_screen_info_set);
  bool HandleAaaaEvent(const mdns::AaaaEvent& aaaa_event,
                       PendingScreenInfoSet* pending_screen_info_set);

  HostInfo* AddOrGetHostInfo(platform::UdpSocketPtr socket,
                             const mdns::DomainName& domain_name);
  HostInfo* GetHostInfo(platform::UdpSocketPtr socket,
                        const mdns::DomainName& domain_name);
  platform::InterfaceIndex GetInterfaceIndexFromSocket(
      platform::UdpSocketPtr socket) const;

  // Service type separated as service name and service protocol for both
  // listening and publishing (e.g. {"_openscreen", "_udp"}).
  std::array<std::string, 2> service_type_;

  // The following variables all relate to what MdnsResponderService publishes,
  // if anything.
  std::string service_hostname_;
  std::string service_instance_name_;
  uint16_t service_port_;
  std::vector<platform::InterfaceIndex> interface_index_whitelist_;
  std::map<std::string, std::string> service_txt_data_;

  std::unique_ptr<MdnsResponderAdapterFactory> mdns_responder_factory_;
  std::unique_ptr<mdns::MdnsResponderAdapter> mdns_responder_;
  std::unique_ptr<MdnsPlatformService> platform_;
  std::vector<MdnsPlatformService::BoundInterface> bound_interfaces_;

  // A map of service information collected from PTR, SRV, and TXT records.  It
  // is keyed by service instance names.
  std::map<mdns::DomainName,
           std::unique_ptr<ServiceInstance>,
           mdns::DomainNameComparator>
      services_by_name_;

  // The first map level indicates to which interface the address records
  // belong.  The second  map level is hostnames to IPAddresses which also
  // includes pointers to dependent service instances.  The service instance
  // pointers act as a reference count to keep the A/AAAA queries alive, when
  // more than one service refers to the same hostname.  This is not currently
  // used by openscreen, but is used by Cast, so may be supported in openscreen
  // in the future.
  std::map<platform::UdpSocketPtr,
           std::map<mdns::DomainName, HostInfo, mdns::DomainNameComparator>>
      network_to_domain_to_host_;

  std::map<std::string, ScreenInfo> screen_info_;
};

}  // namespace openscreen

#endif  // API_IMPL_MDNS_RESPONDER_SERVICE_H_
