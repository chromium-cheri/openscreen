// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_MDNS_RESPONDER_SERVICE_H_
#define API_IMPL_MDNS_RESPONDER_SERVICE_H_

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/impl/mdns_platform_service.h"
#include "api/impl/screen_listener_impl.h"
#include "api/impl/screen_publisher_impl.h"
#include "base/ip_address.h"
#include "discovery/mdns/mdns_responder_adapter.h"
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
      const std::string& service_type,
      std::unique_ptr<MdnsResponderAdapterFactory> mdns_responder_factory,
      std::unique_ptr<MdnsPlatformService> platform);
  ~MdnsResponderService() override;

  void SetServiceConfig(const std::string& hostname,
                        const std::string& instance,
                        uint16_t port,
                        const std::vector<int32_t> interface_index_whitelist,
                        const std::vector<std::string>& txt_lines);

  void HandleNewEvents(platform::ReceivedData data);

  // ScreenListenerImpl::Delegate overrides.
  void StartListener() override;
  void StartAndSuspendListener() override;
  void StopListener() override;
  void SuspendListener() override;
  void ResumeListener() override;
  void SearchNow(ScreenListenerState from) override;

  // ScreenPublisherImpl::Delegate overrides.
  void StartPublisher() override;
  void StartAndSuspendPublisher() override;
  void StopPublisher() override;
  void SuspendPublisher() override;
  void ResumePublisher() override;
  void UpdateFriendlyName(const std::string& friendly_name) override;

 private:
  // NOTE: service_instance implicit in map key
  struct ServiceInstance {
    int32_t ptr_interface_index = 0;
    mdns::DomainName domain_name;
    uint16_t port = 0;
    std::vector<std::string> txt_info;
  };

  // NOTE: domain_name implicit in map key
  struct ServiceAddresses {
    IPv4Address v4_address;
    IPv6Address v6_address;
  };

  void HandleMdnsEvents();
  void StartListening();
  void StopListening();
  void StartService();
  void StopService();
  void StopMdnsResponder();
  void PushScreenInfo(const mdns::DomainName& service_instance,
                      const ServiceInstance& instance_info,
                      const ServiceAddresses& addresses);
  void MaybePushScreenInfo(const mdns::DomainName& service_instance,
                           const ServiceInstance& instance_info);
  void MaybePushScreenInfo(const mdns::DomainName& domain_name,
                           const ServiceAddresses& address_info);
  void RemoveScreenInfo(const mdns::DomainName& service_instance);
  void RemoveScreenInfoByDomain(const mdns::DomainName& domain_name);

  std::array<std::string, 2> service_type_;
  std::string hostname_;
  std::string instance_;
  uint16_t port_;
  std::vector<int32_t> interface_index_whitelist_;
  std::vector<std::string> txt_lines_;
  std::unique_ptr<MdnsResponderAdapterFactory> mdns_responder_factory_;
  std::unique_ptr<mdns::MdnsResponderAdapter> mdns_responder_;
  std::unique_ptr<MdnsPlatformService> platform_;
  MdnsPlatformService::BoundInterfaces bound_interfaces_;

  std::map<mdns::DomainName, ServiceInstance, mdns::DomainNameComparator>
      services_;
  std::map<mdns::DomainName, ServiceAddresses, mdns::DomainNameComparator>
      addresses_;

  std::map<std::string, ScreenInfo> screen_info_;
};

}  // namespace openscreen

#endif  // API_IMPL_MDNS_RESPONDER_SERVICE_H_
