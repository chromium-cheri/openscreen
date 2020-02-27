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

MdnsService::SupportedNetworkEndpoints GetEndpointTypes(
    const Config::NetworkInfo& network_interface) {
  MdnsService::SupportedNetworkEndpoints supported_types =
      MdnsService::SupportedNetworkEndpoints::kNone;
  if (network_interface.v4) {
    supported_types =
        supported_types | MdnsService::SupportedNetworkEndpoints::kV4;
  }
  if (network_interface.v6) {
    supported_types =
        supported_types | MdnsService::SupportedNetworkEndpoints::kV6;
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
                                        network_interface.interface,
                                        GetEndpointTypes(network_interface))),
      querier_(mdns_service_.get(), task_runner_, *this),
      publisher_(mdns_service_.get(), reporting_client, task_runner_, *this),
      network_interface_(network_interface.interface),
      address_v4_(network_interface.v4),
      address_v6_(network_interface.v6) {}

ServiceInstance::~ServiceInstance() {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
}

}  // namespace discovery
}  // namespace openscreen
