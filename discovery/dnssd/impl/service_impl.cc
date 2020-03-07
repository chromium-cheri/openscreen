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
namespace {

MdnsService::SupportedNetworkAddressFamily GetSupportedEndpointTypes(
    const Config::NetworkInfo& interface) {
  MdnsService::SupportedNetworkAddressFamily supported_types =
      MdnsService::kNoAddressFamily;
  if (interface.use_ipv4) {
    supported_types = supported_types | MdnsService::kUseIpV4Multicast;
  }
  if (interface.use_ipv6) {
    supported_types = supported_types | MdnsService::kUseIpV6Multicast;
  }
  return supported_types;
}

}  // namespace

// static
SerialDeletePtr<DnsSdService> DnsSdService::Create(
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
          GetSupportedEndpointTypes(config.network_config[0]))),
      querier_(mdns_service_.get(), task_runner_, *this),
      publisher_(mdns_service_.get(), reporting_client, task_runner_, *this) {
  OSP_DCHECK_EQ(config.network_config.size(), 1);
  network_interface_ = config.network_config[0].interface.index;
  address_v4_ = config.network_config[0].interface.GetIpAddressV4();
  address_v4_ = config.network_config[0].interface.GetIpAddressV6();
}

ServiceImpl::~ServiceImpl() {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
}

}  // namespace discovery
}  // namespace openscreen
