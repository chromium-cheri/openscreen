// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_RESPONDER_ADAPTER_IMPL_H_
#define DISCOVERY_MDNS_MDNS_RESPONDER_ADAPTER_IMPL_H_

#include <map>
#include <memory>
#include <vector>

#include "discovery/mdns/mdns_responder_adapter.h"
#include "platform/api/socket.h"
#include "third_party/mDNSResponder/src/mDNSCore/mDNSEmbeddedAPI.h"

namespace openscreen {
namespace mdns {

class MdnsResponderAdapterImpl final : public MdnsResponderAdapter {
 public:
  static constexpr int kRrCacheSize = 500;

  MdnsResponderAdapterImpl();
  ~MdnsResponderAdapterImpl() override;

  bool Init() override;
  void Close() override;

  void SetHostLabel(const std::string& host_label) override;

  bool RegisterInterface(const platform::InterfaceInfo& interface_info,
                         const platform::IPv4Subnet& interface_address,
                         platform::UdpSocketIPv4Ptr socket,
                         bool advertise) override;
  bool RegisterInterface(const platform::InterfaceInfo& interface_info,
                         const platform::IPv6Subnet& interface_address,
                         platform::UdpSocketIPv6Ptr socket,
                         bool advertise) override;
  bool DeregisterInterface(platform::UdpSocketIPv4Ptr socket) override;
  bool DeregisterInterface(platform::UdpSocketIPv6Ptr socket) override;

  void OnDataReceived(const IPv4Endpoint& source,
                      const IPv4Endpoint& original_destination,
                      const uint8_t* data,
                      int32_t length,
                      platform::UdpSocketIPv4Ptr receiving_socket) override;
  void OnDataReceived(const IPv6Endpoint& source,
                      const IPv6Endpoint& original_destination,
                      const uint8_t* data,
                      int32_t length,
                      platform::UdpSocketIPv6Ptr receiving_socket) override;
  int Execute() override;

  std::vector<AResponseEvent> TakeAResponses() override;
  std::vector<AaaaResponseEvent> TakeAaaaResponses() override;
  std::vector<PtrResponseEvent> TakePtrResponses() override;
  std::vector<SrvResponseEvent> TakeSrvResponses() override;
  std::vector<TxtResponseEvent> TakeTxtResponses() override;

  MdnsResponderError StartAQuery(const DomainName& domain_name) override;
  MdnsResponderError StartAaaaQuery(const DomainName& domain_name) override;
  MdnsResponderError StartPtrQuery(const DomainName& service_type) override;
  MdnsResponderError StartSrvQuery(const DomainName& service_instance) override;
  MdnsResponderError StartTxtQuery(const DomainName& service_instance) override;

  MdnsResponderError StopAQuery(const DomainName& domain_name) override;
  MdnsResponderError StopAaaaQuery(const DomainName& domain_name) override;
  MdnsResponderError StopPtrQuery(const DomainName& service_type) override;
  MdnsResponderError StopSrvQuery(const DomainName& service_instance) override;
  MdnsResponderError StopTxtQuery(const DomainName& service_instance) override;

  MdnsResponderError RegisterService(
      const std::string& service_name,
      const DomainName& service_type,
      const DomainName& target_host,
      uint16_t target_port,
      const std::vector<std::string>& lines) override;

 private:
  static void AQueryCallback(mDNS* m,
                             DNSQuestion* question,
                             const ResourceRecord* answer,
                             QC_result added);
  static void AaaaQueryCallback(mDNS* m,
                                DNSQuestion* question,
                                const ResourceRecord* answer,
                                QC_result added);
  static void PtrQueryCallback(mDNS* m,
                               DNSQuestion* question,
                               const ResourceRecord* answer,
                               QC_result added);
  static void SrvQueryCallback(mDNS* m,
                               DNSQuestion* question,
                               const ResourceRecord* answer,
                               QC_result added);
  static void TxtQueryCallback(mDNS* m,
                               DNSQuestion* question,
                               const ResourceRecord* answer,
                               QC_result added);
  static void ServiceCallback(mDNS* m,
                              ServiceRecordSet* service_record,
                              mStatus result);

  class DomainNameComparator {
   public:
    bool operator()(const DomainName& a, const DomainName& b) const;
  };

  CacheEntity rr_cache_[kRrCacheSize];
  mDNS mdns_;
  mDNS_PlatformSupport platform_storage_;

  std::map<DomainName, DNSQuestion, DomainNameComparator> a_questions_;
  std::map<DomainName, DNSQuestion, DomainNameComparator> aaaa_questions_;
  std::map<DomainName, DNSQuestion, DomainNameComparator> ptr_questions_;
  std::map<DomainName, DNSQuestion, DomainNameComparator> srv_questions_;
  std::map<DomainName, DNSQuestion, DomainNameComparator> txt_questions_;

  std::map<platform::UdpSocketIPv4Ptr, NetworkInterfaceInfo>
      v4_responder_interface_info_;
  std::map<platform::UdpSocketIPv6Ptr, NetworkInterfaceInfo>
      v6_responder_interface_info_;

  std::vector<AResponseEvent> a_responses_;
  std::vector<AaaaResponseEvent> aaaa_responses_;
  std::vector<PtrResponseEvent> ptr_responses_;
  std::vector<SrvResponseEvent> srv_responses_;
  std::vector<TxtResponseEvent> txt_responses_;

  std::vector<std::unique_ptr<ServiceRecordSet>> service_records_;
};

}  // namespace mdns
}  // namespace openscreen

#endif
