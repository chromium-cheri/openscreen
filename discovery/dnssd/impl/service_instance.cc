// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/service_instance.h"

#include <utility>

#include "discovery/mdns/public/mdns_service.h"
#include "platform/api/task_runner.h"

namespace openscreen {
namespace discovery {
namespace {

MdnsService::SupportedNetworkAddressFamily GetAddressFamilies(
    const Config::NetworkInfo& network_interface) {
  MdnsService::SupportedNetworkAddressFamily supported_types =
      MdnsService::kNoAddressFamily;
  if (network_interface.use_ipv4) {
    supported_types = supported_types | MdnsService::kUseIpV4Multicast;
  }
  if (network_interface.use_ipv6) {
    supported_types = supported_types | MdnsService::kUseIpV6Multicast;
  }

  return supported_types;
}

}  // namespace

ServiceInstance::ServiceInstance(TaskRunner* task_runner,
                                 ReportingClient* reporting_client,
                                 const Config& config,
                                 const Config::NetworkInfo& network_interface)
    : task_runner_(task_runner),
      mdns_service_(MdnsService::Create(task_runner,
                                        reporting_client,
                                        config,
                                        network_interface.interface.index,
                                        GetAddressFamilies(network_interface))),
      querier_(mdns_service_.get(), task_runner_, *this),
      publisher_(mdns_service_.get(), reporting_client, task_runner_, *this),
      network_interface_(network_interface.interface.index),
      address_v4_(network_interface.interface.GetIpAddressV4()),
      address_v6_(network_interface.interface.GetIpAddressV6()) {
  // Check an address family is only used if an associated address is available.
  OSP_DCHECK(!network_interface.use_ipv4 || address_v4_);
  OSP_DCHECK(!network_interface.use_ipv6 || address_v6_);
}

ServiceInstance::~ServiceInstance() {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
}

}  // namespace discovery
}  // namespace openscreen
