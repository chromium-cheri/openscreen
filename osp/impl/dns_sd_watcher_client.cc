// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/dns_sd_watcher_client.h"

#include <utility>

#include "discovery/common/config.h"
#include "discovery/public/dns_sd_service_factory.h"
#include "platform/base/error.h"
#include "platform/base/interface_info.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace osp {

using State = ServiceListener::State;

namespace {

ErrorOr<ServiceInfo> DnsSdInstanceEndpointToServiceInfo(
    const discovery::DnsSdInstanceEndpoint& end_point) {
  if (end_point.endpoints().empty() ||
      end_point.network_interface() == kInvalidNetworkInterfaceIndex) {
    return Error::Code::kParameterInvalid;
  }

  ServiceInfo service_info{end_point.service_id(), end_point.instance_id(),
                           end_point.network_interface()};
  if (end_point.endpoints()[0].address.IsV4()) {
    service_info.v4_endpoint = end_point.endpoints()[0];
  } else {
    service_info.v6_endpoint = end_point.endpoints()[0];
  }
  return service_info;
}

}  // namespace

DnsSdWatcherClient::DnsSdWatcherClient(openscreen::TaskRunner* task_runner)
    : task_runner_(task_runner) {
  OSP_DCHECK(task_runner_);
}

DnsSdWatcherClient::~DnsSdWatcherClient() = default;

void DnsSdWatcherClient::StartListener(const ServiceListener::Config& config) {
  OSP_LOG_INFO << "StartListener with " << config.network_interfaces.size()
               << " interfaces";
  StartWatcherInternal(config);
  dns_sd_watcher_->StartDiscovery();
  SetState(State::kRunning);
}

void DnsSdWatcherClient::StartAndSuspendListener(
    const ServiceListener::Config& config) {
  StartWatcherInternal(config);
  SetState(State::kSuspended);
}

void DnsSdWatcherClient::StopListener() {
  dns_sd_watcher_.reset();
  SetState(State::kStopped);
}

void DnsSdWatcherClient::SuspendListener() {
  OSP_DCHECK(dns_sd_watcher_);
  dns_sd_watcher_->StopDiscovery();
  SetState(State::kSuspended);
}

void DnsSdWatcherClient::ResumeListener() {
  OSP_DCHECK(dns_sd_watcher_);
  dns_sd_watcher_->StartDiscovery();
  SetState(State::kRunning);
}

void DnsSdWatcherClient::SearchNow(State from) {
  OSP_DCHECK(dns_sd_watcher_);
  if (from == State::kSuspended) {
    dns_sd_watcher_->StartDiscovery();
  }

  dns_sd_watcher_->DiscoverNow();
  SetState(State::kSearching);
}

void DnsSdWatcherClient::StartWatcherInternal(
    const ServiceListener::Config& config) {
  OSP_DCHECK(!dns_sd_watcher_);
  if (!dns_sd_service_) {
    dns_sd_service_ = CreateDnsSdServiceInternal(config);
  }
  dns_sd_watcher_ = std::make_unique<OspDnsSdWatcher>(
      dns_sd_service_.get(), kOpenScreenServiceName,
      DnsSdInstanceEndpointToServiceInfo,
      [this](const ServiceInfo& service_info,
             OspDnsSdWatcher::ServiceChanged reason) {
        OnServiceChanged(service_info, reason);
      });
}

SerialDeletePtr<discovery::DnsSdService>
DnsSdWatcherClient::CreateDnsSdServiceInternal(
    const ServiceListener::Config& config) {
  // NOTE: With the current API, the client cannot customize the behavior of
  // DNS-SD beyond the interface list.
  openscreen::discovery::Config dns_sd_config;
  dns_sd_config.enable_publication = false;
  dns_sd_config.network_info = config.network_interfaces;

  // NOTE:
  // It's desirable for the DNS-SD publisher and the DNS-SD listener for OSP to
  // share the underlying mDNS socket and state, to avoid the agent from
  // binding 2 sockets per network interface.
  //
  // This can be accomplished by having the agent use a shared instance of the
  // discovery::DnsSdService, e.g. through a ref-counting handle, so that the
  // OSP publisher and the OSP listener don't have to coordinate through an
  // additional object.
  return CreateDnsSdService(task_runner_, listener_, dns_sd_config);
}

void DnsSdWatcherClient::OnServiceChanged(
    const ServiceInfo& service_info,
    OspDnsSdWatcher::ServiceChanged reason) {
  switch (reason) {
    case OspDnsSdWatcher::ServiceChanged::kCreated:
      listener_->OnReceiverAdded(service_info);
    case OspDnsSdWatcher::ServiceChanged::kUpdated:
      listener_->OnReceiverChanged(service_info);
    case OspDnsSdWatcher::ServiceChanged::kDeleted:
      listener_->OnReceiverRemoved(service_info);
    case OspDnsSdWatcher::ServiceChanged::KCleared:
      listener_->OnAllReceiversRemoved();
    default:
      OSP_NOTREACHED();
  }
}

}  // namespace osp
}  // namespace openscreen
