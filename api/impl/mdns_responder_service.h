// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_MDNS_RESPONDER_SERVICE_H_
#define API_IMPL_MDNS_RESPONDER_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/impl/platform_service.h"
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
      std::unique_ptr<PlatformService> platform);
  ~MdnsResponderService() override;

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
    bool has_ptr = true;
    mdns::DomainName domain_name;
    uint16_t port = 0;
    std::vector<std::string> txt_info;
  };

  // NOTE: domain_name implicit in map key
  struct ServiceAddresses {
    IPv4Address v4_address;
    IPv6Address v6_address;
  };

  class DomainNameComparator {
   public:
    bool operator()(const mdns::DomainName& a, const mdns::DomainName& b) const;
  };

  void HandleMdnsEvents();
  void PushScreenInfo(const mdns::DomainName& service_instance,
                      const ServiceInstance& instance_info,
                      const ServiceAddresses& addresses);
  void MaybePushScreenInfo(const mdns::DomainName& service_instance,
                           const ServiceInstance& instance_info);
  void MaybePushScreenInfo(const mdns::DomainName& domain_name,
                           const ServiceAddresses& address_info);
  void RemoveScreenInfo(const mdns::DomainName& service_instance);
  void RemoveScreenInfoByDomain(const mdns::DomainName& domain_name);

  const std::string service_type_;
  std::unique_ptr<MdnsResponderAdapterFactory> mdns_responder_factory_;
  std::unique_ptr<mdns::MdnsResponderAdapter> mdns_responder_;
  std::unique_ptr<PlatformService> platform_;

  std::map<mdns::DomainName, ServiceInstance, DomainNameComparator> services_;
  std::map<mdns::DomainName, ServiceAddresses, DomainNameComparator> addresses_;

  std::map<std::string, ScreenInfo> screen_info_;
};

}  // namespace openscreen

#endif  // API_IMPL_MDNS_RESPONDER_SERVICE_H_
