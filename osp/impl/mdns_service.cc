// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/mdns_service.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "platform/base/error.h"
#include "util/osp_logging.h"
#include "util/trace_logging.h"

namespace openscreen {

// 1. Make a Config to instantiate the DnsSdService.
//    This is basically a list of network interfaces passed through the
//    ServicePublisher config object.
// 2. Write a converter between osp::ServiceInfo and discovery::DnsSdInstance.
//    We will need additional info from the OSP Config such as the friendly
//    name.
// 3. Lazily instantiate a DnsSdServiceWatcher and DnsSdServicePublisher
//    and use them to implement the public API below.

namespace {

discovery::Config MakeDiscoveryConfig(
    const ServicePublisher::Config& osp_config) {
  discovery::Config config;
  std::vector<InterfaceInfo> interfaces;
  if (osp_config.network_interfaces.empty()) {
    interfaces = GetNetworkInterfaces();
  } else {
    interfaces = osp_config.network_interfaces;
  }
  for (const InterfaceInfo& interface : interfaces) {
    discovery::Config::NetworkInfo net_info;
    net_info.interface = interface;
    net_info.supported_address_families =
        discovery::Config::NetworkInfo::kNoAddressFamily;
    if (interface.GetIpAddressV4()) {
      net_info.supported_address_families |=
          discovery::Config::NetworkInfo::kUseIpV4;
    }
    if (interface.GetIpAddressV6()) {
      net_info.supported_address_families |=
          discovery::Config::NetworkInfo::kUseIpV6;
    }
    OSP_DCHECK(net_info.supported_address_families !=
               discovery::Config::NetworkInfo::kNoAddressFamily);
    config.push_back(interface);
  }
  config.enable_publishing = true;
  config.enable_querying = true;
  return config;
}

}  // namespace

namespace osp {

MdnsService::MdnsService(TaskRunner* task_runner,
                         const std::string& service_name,
                         const std::string& service_protocol)
    : service_type_{{service_name, service_protocol}},
      task_runner_(task_runner),
      dns_sd_service_(discovery::CreateDnsSdService(task_runner,
                                                    this,
                                                    MakeDiscoveryConfig())) {}

MdnsService::~MdnsService() = default;

void MdnsService::SetServiceConfig(
    const std::string& hostname,
    const std::string& instance,
    uint16_t port,
    const std::vector<NetworkInterfaceIndex> allowlist,
    const std::map<std::string, std::string>& txt_data) {
  OSP_DCHECK(!hostname.empty());
  OSP_DCHECK(!instance.empty());
  OSP_DCHECK_NE(0, port);
  service_hostname_ = hostname;
  service_instance_name_ = instance;
  service_port_ = port;
  interface_index_allowlist_ = allowlist;
  service_txt_data_ = txt_data;
}

void MdnsService::StartListener() {
  StartListening();
  ServiceListenerImpl::Delegate::SetState(ServiceListener::State::kRunning);
}

void MdnsService::StartAndSuspendListener() {
  ServiceListenerImpl::Delegate::SetState(ServiceListener::State::kSuspended);
}

void MdnsService::StopListener() {
  StopListening();
  ServiceListenerImpl::Delegate::SetState(ServiceListener::State::kStopped);
}

void MdnsService::SuspendListener() {
  StopListening();
  ServiceListenerImpl::Delegate::SetState(ServiceListener::State::kSuspended);
}

void MdnsService::ResumeListener() {
  StartListening();
  ServiceListenerImpl::Delegate::SetState(ServiceListener::State::kRunning);
}

void MdnsService::SearchNow(ServiceListener::State from) {
  // Check current state???
  dns_sd_service_->GetQuerier()->ReinitializeQueries(kOpenScreenServiceName);
  ServiceListenerImpl::Delegate::SetState(from);
}

void MdnsService::StartPublisher() {
  dns_sd_service_->GetPublisher()->Register(service_instance_, this);
  ServicePublisherImpl::Delegate::SetState(ServicePublisher::State::kRunning);
}

void MdnsService::StartAndSuspendPublisher() {
  ServicePublisherImpl::Delegate::SetState(ServicePublisher::State::kSuspended);
}

void MdnsService::StopPublisher() {
  dns_sd_service_->GetPublisher()->DeregisterAll(kOpenScreenServiceName);
  ServicePublisherImpl::Delegate::SetState(ServicePublisher::State::kStopped);
}

void MdnsService::SuspendPublisher() {
  dns_sd_service_->GetPublisher()->DeregisterAll(kOpenScreenServiceName);
  ServicePublisherImpl::Delegate::SetState(ServicePublisher::State::kSuspended);
}

void MdnsService::ResumePublisher() {
  dns_sd_service_->GetPublisher()->Register(service_instance_, this);
}

}  // namespace osp
}  // namespace openscreen
