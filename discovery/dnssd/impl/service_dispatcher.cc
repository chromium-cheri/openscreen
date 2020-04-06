// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/service_dispatcher.h"

#include <utility>

#include "discovery/common/config.h"
#include "discovery/dnssd/impl/service_instance.h"
#include "discovery/dnssd/public/dns_sd_instance.h"
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
      task_runner,
      new ServiceDispatcher(task_runner, reporting_client, config));
}

ServiceDispatcher::ServiceDispatcher(TaskRunner* task_runner,
                                     ReportingClient* reporting_client,
                                     const Config& config)
    : task_runner_(task_runner),
      publisher_(config.enable_publication ? this : nullptr),
      querier_(config.enable_querying ? this : nullptr) {
  OSP_DCHECK_GT(config.network_info.size(), 0);
  OSP_DCHECK(task_runner);

  service_instances_.reserve(config.network_info.size());
  for (const auto& network_info : config.network_info) {
    service_instances_.push_back(std::make_unique<ServiceInstance>(
        task_runner_, reporting_client, config, network_info));
  }
}

ServiceDispatcher::~ServiceDispatcher() {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
}

// DnsSdQuerier overrides.
void ServiceDispatcher::StartQuery(const std::string& service, Callback* cb) {
  for (auto& service_instance : service_instances_) {
    auto* querier = service_instance->GetQuerier();
    OSP_CHECK(querier);

    querier->StartQuery(service, cb);
  }
}

void ServiceDispatcher::StopQuery(const std::string& service, Callback* cb) {
  for (auto& service_instance : service_instances_) {
    auto* querier = service_instance->GetQuerier();
    OSP_CHECK(querier);

    querier->StopQuery(service, cb);
  }
}

void ServiceDispatcher::ReinitializeQueries(const std::string& service) {
  for (auto& service_instance : service_instances_) {
    auto* querier = service_instance->GetQuerier();
    OSP_CHECK(querier);

    querier->ReinitializeQueries(service);
  }
}

// DnsSdPublisher overrides.
Error ServiceDispatcher::Register(const DnsSdInstance& instance,
                                  Client* client) {
  // TODO(crbug.com/openscreen/114): Trace each below call so multiple errors
  // can be seen.
  Error result = Error::None();
  for (auto& service_instance : service_instances_) {
    auto* publisher = service_instance->GetPublisher();
    OSP_CHECK(publisher);

    Error inner_result = publisher->Register(instance, client);
    if (!inner_result.ok()) {
      result = std::move(inner_result);
    }
  }
  return result;
}

Error ServiceDispatcher::UpdateRegistration(const DnsSdInstance& instance) {
  // TODO(crbug.com/openscreen/114): Trace each below call so multiple errors
  // can be seen.
  Error result = Error::None();
  for (auto& service_instance : service_instances_) {
    auto* publisher = service_instance->GetPublisher();
    OSP_CHECK(publisher);

    Error inner_result = publisher->UpdateRegistration(instance);
    if (!inner_result.ok()) {
      result = std::move(inner_result);
    }
  }
  return result;
}

ErrorOr<int> ServiceDispatcher::DeregisterAll(const std::string& service) {
  // TODO(crbug.com/openscreen/114): Trace each below call so multiple errors
  // can be seen.
  int total = 0;
  Error failure = Error::None();
  for (auto& service_instance : service_instances_) {
    auto* publisher = service_instance->GetPublisher();
    OSP_CHECK(publisher);

    auto result = publisher->DeregisterAll(service);
    if (result.is_error()) {
      failure = std::move(result.error());
    } else {
      total += result.value();
    }
  }

  if (!failure.ok()) {
    return failure;
  } else {
    return total;
  }
}

}  // namespace discovery
}  // namespace openscreen
