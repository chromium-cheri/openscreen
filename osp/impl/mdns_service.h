// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_MDNS_SERVICE_H_
#define OSP_IMPL_MDNS_SERVICE_H_

#include <array>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "osp/impl/service_listener_impl.h"
#include "osp/impl/service_publisher_impl.h"
#include "platform/api/network_interface.h"
#include "platform/api/task_runner.h"
#include "platform/api/time.h"
#include "platform/base/ip_address.h"
#include "util/alarm.h"

namespace openscreen {
namespace osp {

class MdnsService : public ServiceListenerImpl::Delegate,
                    public ServicePublisherImpl::Delegate {
 public:
  MdnsService(TaskRunner* task_runner,
              const std::string& service_name,
              const std::string& service_protocol);
  ~MdnsService() override;

  void SetServiceConfig(const std::string& hostname,
                        const std::string& instance,
                        uint16_t port,
                        const std::vector<NetworkInterfaceIndex> allowlist,
                        const std::map<std::string, std::string>& txt_data);

  // ServiceListenerImpl::Delegate overrides.
  void StartListener() override;
  void StartAndSuspendListener() override;
  void StopListener() override;
  void SuspendListener() override;
  void ResumeListener() override;
  void SearchNow(ServiceListener::State from) override;

  // ServicePublisherImpl::Delegate overrides.
  void StartPublisher() override;
  void StartAndSuspendPublisher() override;
  void StopPublisher() override;
  void SuspendPublisher() override;
  void ResumePublisher() override;

 protected:
 private:
  std::unique_ptr<discovery::DnsSdServiceWatcher> service_watcher_;
  std::unique_ptr<discovery::DnsSdServicePublisher> service_publisher_;
  SerialDeletePtr<discovery::DnsSdService> dns_sd_service_;

  void StartListening();
  void StopListening();
  void StartService();
  void StopService();
  void StopMdnsResponder();
  void UpdatePendingServiceInfoSet(InstanceNameSet* modified_instance_names,
                                   const DomainName& domain_name);
  void RemoveAllReceivers();

  // Service type separated as service name and service protocol for both
  // listening and publishing (e.g. {"_openscreen", "_udp"}).
  std::array<std::string, 2> service_type_;

  // The following variables all relate to what MdnsService publishes,
  // if anything.
  std::string service_hostname_;
  std::string service_instance_name_;
  uint16_t service_port_;
  std::vector<NetworkInterfaceIndex> interface_index_allowlist_;
  std::map<std::string, std::string> service_txt_data_;

  std::map<std::string, ServiceInfo> receiver_info_;

  TaskRunner* const task_runner_;

  friend class TestingMdnsService;
};

}  // namespace osp
}  // namespace openscreen

#endif  // OSP_IMPL_MDNS_SERVICE_H_
