// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/service_dispatcher.h"

#include <utility>

#include "discovery/common/config.h"
#include "discovery/mdns/public/mdns_service.h"
#include "platform/api/task_runner.h"

namespace openscreen {
namespace discovery {

// static
SerialDeletePtr<DnsSdService> DnsSdService::Create(
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
    : task_runner_(task_runner) {
  service_instances_.reserve(config.network_config.size());
  for (const auto& network_info : config.network_config) {
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
    if (querier) {
      querier->StartQuery(service, cb);
    }
  }
}

void ServiceDispatcher::StopQuery(const std::string& service, Callback* cb) {
  for (auto& service_instance : service_instances_) {
    auto* querier = service_instance->GetQuerier();
    if (querier) {
      querier->StopQuery(service, cb);
    }
  }
}

void ServiceDispatcher::ReinitializeQueries(const std::string& service) {
  for (auto& service_instance : service_instances_) {
    auto* querier = service_instance->GetQuerier();
    if (querier) {
      querier->ReinitializeQueries(service);
    }
  }
}

// DnsSdPublisher overrides.
Error ServiceDispatcher::Register(const DnsSdInstanceRecord& record,
                                  Client* client) {
  Error result = Error::None();
  for (auto& service_instance : service_instances_) {
    auto* publisher = service_instance->GetPublisher();
    if (publisher) {
      Error inner_result = publisher->Register(record, client);
      if (!inner_result.ok()) {
        result = inner_result;
      }
    }
  }
  return result;
}

Error ServiceDispatcher::UpdateRegistration(const DnsSdInstanceRecord& record) {
  Error result = Error::None();
  for (auto& service_instance : service_instances_) {
    auto* publisher = service_instance->GetPublisher();
    if (publisher) {
      Error inner_result = publisher->UpdateRegistration(record);
      if (!inner_result.ok()) {
        result = inner_result;
      }
    }
  }
  return result;
}

int ServiceDispatcher::DeregisterAll(const std::string& service) {
  int total = 0;
  for (auto& service_instance : service_instances_) {
    auto* publisher = service_instance->GetPublisher();
    if (publisher) {
      total += publisher->DeregisterAll(service);
    }
  }
  return total;
}

}  // namespace discovery
}  // namespace openscreen
