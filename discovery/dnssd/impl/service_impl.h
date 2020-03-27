// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_IMPL_SERVICE_IMPL_H_
#define DISCOVERY_DNSSD_IMPL_SERVICE_IMPL_H_

#include <memory>

#include "discovery/dnssd/impl/network_config.h"
#include "discovery/dnssd/impl/publisher_impl.h"
#include "discovery/dnssd/impl/querier_impl.h"
#include "discovery/dnssd/public/dns_sd_service.h"
#include "platform/base/interface_info.h"

namespace openscreen {

class TaskRunner;

namespace discovery {

class MdnsService;

class ServiceImpl final : public DnsSdService, public NetworkConfig {
 public:
  ServiceImpl(TaskRunner* task_runner,
              ReportingClient* reporting_client,
              const Config& config);
  ~ServiceImpl() override;

  // DnsSdService overrides.
  DnsSdQuerier* GetQuerier() override { return querier_.get(); }
  DnsSdPublisher* GetPublisher() override { return publisher_.get(); }

  // NetworkConfig overrides.
  NetworkInterfaceIndex network_interface() const override {
    return network_interface_;
  }
  const IPAddress& address_v4() const override { return address_v4_; }
  const IPAddress& address_v6() const override { return address_v6_; }

 private:
  TaskRunner* const task_runner_;

  std::unique_ptr<MdnsService> mdns_service_;

  std::unique_ptr<QuerierImpl> querier_;
  std::unique_ptr<PublisherImpl> publisher_;

  NetworkInterfaceIndex network_interface_;
  IPAddress address_v4_;
  IPAddress address_v6_;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_IMPL_SERVICE_IMPL_H_
