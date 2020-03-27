// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/service_impl.h"

#include <utility>

#include "discovery/common/config.h"
#include "discovery/mdns/public/mdns_service.h"
#include "platform/api/task_runner.h"

namespace openscreen {
namespace discovery {

// static
SerialDeletePtr<DnsSdService> CreateDnsSdService(
    TaskRunner* task_runner,
    ReportingClient* reporting_client,
    const Config& config) {
  return SerialDeletePtr<DnsSdService>(
      task_runner, new ServiceImpl(task_runner, reporting_client, config));
}

ServiceImpl::ServiceImpl(TaskRunner* task_runner,
                         ReportingClient* reporting_client,
                         const Config& config)
    : task_runner_(task_runner),
      mdns_service_(MdnsService::Create(
          task_runner,
          reporting_client,
          config,
          config.network_config[0].interface.index,
          config.network_config[0].supported_address_families)) {
  OSP_DCHECK_EQ(config.network_config.size(), 1);
  network_interface_ = config.network_config[0].interface.index;
  const auto supported_address_families =
      config.network_config[0].supported_address_families;

  if (supported_address_families & Config::NetworkInfo::kUseIpV4Multicast) {
    address_v4_ = config.network_config[0].interface.GetIpAddressV4();
    OSP_DCHECK(address_v4_);
  }

  if (supported_address_families & Config::NetworkInfo::kUseIpV6Multicast) {
    address_v6_ = config.network_config[0].interface.GetIpAddressV6();
    OSP_DCHECK(address_v6_);
  }

  if (config.enable_querying) {
    querier_ =
        std::make_unique<QuerierImpl>(mdns_service_.get(), task_runner_, *this);
  }
  if (config.enable_publication) {
    publisher_ = std::make_unique<PublisherImpl>(
        mdns_service_.get(), reporting_client, task_runner_, *this);
  }
}

ServiceImpl::~ServiceImpl() {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
}

}  // namespace discovery
}  // namespace openscreen
