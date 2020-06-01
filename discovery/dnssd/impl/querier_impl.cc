// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/querier_impl.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "discovery/dnssd/impl/conversion_layer.h"
#include "discovery/dnssd/impl/network_interface_config.h"
#include "platform/api/task_runner.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace discovery {
namespace {

static constexpr char kLocalDomain[] = "local";

bool IsEqualOrUpdate(const absl::optional<DnsSdInstanceEndpoint>& first,
                     const absl::optional<DnsSdInstanceEndpoint>& second) {
  if (!first.has_value()) {
    return second.has_value();
  } else if (second.has_value()) {
    return false;
  }

  const DnsSdInstanceEndpoint& a = first.value();
  const DnsSdInstanceEndpoint& b = second.value();

  return a.network_interface() == b.network_interface() &&
         a.instance_id() == b.instance_id() &&
         a.service_id() == b.service_id() && a.domain_id() == b.domain_id();
}

// Calculates the created, updated, and deleted elements using the provided
// sets, appending these values to the provided vectors.
//
// NOTE: A lot of operations are used to do this, but each is only O(n) so the
// resulting algorithm is still fast.
void CalculateChangeSets(std::vector<DnsSdInstanceEndpoint> old_endpoints,
                         std::vector<DnsSdInstanceEndpoint> new_endpoints,
                         std::vector<DnsSdInstanceEndpoint>* created_out,
                         std::vector<DnsSdInstanceEndpoint>* updated_out,
                         std::vector<DnsSdInstanceEndpoint>* deleted_out) {
  OSP_DCHECK(created_out);
  OSP_DCHECK(updated_out);
  OSP_DCHECK(deleted_out);

  // First, sort the lists so that set operations can be used
  std::sort(new_endpoints.begin(), new_endpoints.end());
  std::sort(old_endpoints.begin(), new_endpoints.end());

  // Use set difference with default operators to find the elements present in
  // one list but not the others.
  //
  // NOTE: Because absl::optional<...> types are used here and below, calls to
  // the ctor and dtor for empty elements are no-ops.
  const int total_count = old_endpoints.size() + new_endpoints.size();
  std::vector<absl::optional<DnsSdInstanceEndpoint>> created_or_updated(
      total_count);
  std::vector<absl::optional<DnsSdInstanceEndpoint>> deleted_or_updated(
      total_count);

  auto new_end = std::set_difference(new_endpoints.begin(), new_endpoints.end(),
                                     old_endpoints.begin(), old_endpoints.end(),
                                     created_or_updated.begin());
  created_or_updated.erase(new_end, created_or_updated.end());

  new_end = std::set_difference(old_endpoints.begin(), old_endpoints.end(),
                                new_endpoints.begin(), new_endpoints.end(),
                                deleted_or_updated.begin());
  deleted_or_updated.erase(new_end, deleted_or_updated.end());

  // Next, find the updated elements.
  const size_t max_count =
      std::max(created_or_updated.size(), deleted_or_updated.size());
  std::vector<absl::optional<DnsSdInstanceEndpoint>> updated(max_count);
  new_end = std::set_intersection(
      created_or_updated.begin(), created_or_updated.end(),
      deleted_or_updated.begin(), deleted_or_updated.end(), updated.begin(),
      IsEqualOrUpdate);
  updated.erase(new_end, updated.end());

  // Use the updated elements to find all created and deleted elements.
  std::vector<absl::optional<DnsSdInstanceEndpoint>> created(
      created_or_updated.size());
  std::vector<absl::optional<DnsSdInstanceEndpoint>> deleted(
      deleted_or_updated.size());

  new_end =
      std::set_difference(created_or_updated.begin(), created_or_updated.end(),
                          updated.begin(), updated.end(), created.begin());
  created.erase(new_end, created.end());

  new_end =
      std::set_difference(deleted_or_updated.begin(), deleted_or_updated.end(),
                          updated.begin(), updated.end(), deleted.begin());
  deleted.erase(new_end, deleted.end());

  // Return the calculated elements back to the caller in the output variables.
  for (absl::optional<DnsSdInstanceEndpoint>& endpoint : created) {
    created_out->push_back(std::move(endpoint.value()));
  }
  for (absl::optional<DnsSdInstanceEndpoint>& endpoint : updated) {
    updated_out->push_back(std::move(endpoint.value()));
  }
  for (absl::optional<DnsSdInstanceEndpoint>& endpoint : deleted) {
    deleted_out->push_back(std::move(endpoint.value()));
  }
}

}  // namespace

QuerierImpl::QuerierImpl(MdnsService* mdns_querier,
                         TaskRunner* task_runner,
                         const NetworkInterfaceConfig* network_config)
    : mdns_querier_(mdns_querier),
      task_runner_(task_runner),
      network_config_(network_config) {
  OSP_DCHECK(mdns_querier_);
  OSP_DCHECK(task_runner_);
  OSP_DCHECK(network_config_);
}

QuerierImpl::~QuerierImpl() = default;

void QuerierImpl::StartQuery(const std::string& service, Callback* callback) {
  OSP_DCHECK(callback);
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  OSP_DVLOG << "Starting DNS-SD query for service '" << service << "'";

  ServiceKey key(service, kLocalDomain);
  DomainName domain = key.GetName();
  if (!graph_->IsTracked(domain)) {
    std::function<void(const DomainName&)> mdns_query(
        [this, &domain](const DomainName& changed_domain) {
          OSP_DVLOG << "Starting mDNS query for '" << domain.ToString() << "'";
          mdns_querier_->StartQuery(changed_domain, DnsType::kANY,
                                    DnsClass::kANY, this);
        });
    graph_->StartTracking(domain, std::move(mdns_query));

    auto it = callback_map_.find(key);
    OSP_DCHECK(it != callback_map_.end());
    it->second.push_back(callback);
  } else {
    ErrorOr<std::vector<DnsSdInstanceEndpoint>> endpoints =
        graph_->CreateEndpoints(DnsDataGraph::DomainGroup::kPtr, domain);
    if (endpoints.is_value()) {
      for (const DnsSdInstanceEndpoint& endpoint : endpoints.value()) {
        callback->OnEndpointCreated(endpoint);
      }
    }

    callback_map_.emplace(std::move(key), std::vector<Callback*>{callback});
  }
}

void QuerierImpl::StopQuery(const std::string& service, Callback* callback) {
  OSP_DCHECK(callback);
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  OSP_DVLOG << "Stopping DNS-SD query for service '" << service << "'";

  ServiceKey key(service, kLocalDomain);
  const auto callbacks_it = callback_map_.find(key);
  if (callbacks_it == callback_map_.end()) {
    return;
  }

  std::vector<Callback*>& callbacks = callbacks_it->second;
  const auto it = std::find(callbacks.begin(), callbacks.end(), callback);
  if (it != callbacks.end()) {
    callbacks.erase(it);
    if (callbacks.empty()) {
      callback_map_.erase(callbacks_it);

      ServiceKey key(service, kLocalDomain);
      DomainName domain = key.GetName();

      std::function<void(const DomainName&)> mdns_query(
          [this, &domain](const DomainName& changed_domain) {
            OSP_DVLOG << "Stopping mDNS query for '" << domain.ToString()
                      << "'";
            mdns_querier_->StopQuery(changed_domain, DnsType::kANY,
                                     DnsClass::kANY, this);
          });
      graph_->StopTracking(domain, std::move(mdns_query));
    }
  }
}

bool QuerierImpl::IsQueryRunning(const std::string& service) const {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  ServiceKey key(service, kLocalDomain);
  return graph_->IsTracked(key.GetName());
}

void QuerierImpl::ReinitializeQueries(const std::string& service) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  OSP_DVLOG << "Re-initializing query for service '" << service << "'";

  const ServiceKey key(service, kLocalDomain);
  DomainName domain = key.GetName();

  std::function<void(const DomainName&)> stop_callback(
      [](const DomainName& domain) {});
  std::function<void(const DomainName&)> start_callback(
      [](const DomainName& domain) {});
  graph_->StopTracking(domain, std::move(stop_callback));
  graph_->StartTracking(domain, std::move(start_callback));

  // Restart top-level queries.
  mdns_querier_->ReinitializeQueries(GetPtrQueryInfo(key).name);
}

std::vector<PendingQueryChange> QuerierImpl::OnRecordChanged(
    const MdnsRecord& record,
    RecordChangedEvent event) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  OSP_DVLOG << "Record with name '" << record.name().ToString()
            << "' and type '" << record.dns_type()
            << "' has received change of type '" << event << "'";

  // Get the current set of DnsSdInstanceEndpoints prior to this change.
  ErrorOr<std::vector<DnsSdInstanceEndpoint>> old_endpoints_or_error =
      graph_->CreateEndpoints(GetDomainGroup(record), record.name());
  if (old_endpoints_or_error.is_error()) {
    OSP_LOG << "Failed to handle " << record.dns_type()
            << " record change of type " << event << " with error "
            << old_endpoints_or_error.error();
    return {};
  }

  // Apply the changes, creating a list of all pending changes that should be
  // applied afterwards.
  ErrorOr<std::vector<PendingQueryChange>> pending_changes_or_error =
      ApplyRecordChanges(record, event);
  if (pending_changes_or_error.is_error()) {
    OSP_LOG << "Failed to apply changes for " << record.dns_type()
            << " record change of type " << event << " with error "
            << pending_changes_or_error.error();
    return {};
  }
  std::vector<PendingQueryChange> pending_changes =
      std::move(pending_changes_or_error.value());

  // Get the new set of DnsSdInstanceEndpoints following this change.
  ErrorOr<std::vector<DnsSdInstanceEndpoint>> new_endpoints_or_error =
      graph_->CreateEndpoints(GetDomainGroup(record), record.name());
  if (new_endpoints_or_error.is_error()) {
    OSP_LOG << "Failed to handle " << record.dns_type()
            << " record change of type " << event << " with error "
            << new_endpoints_or_error.error();
    return pending_changes;
  }

  // Get all callbacks. If there are none, no further processing is needed.
  auto& old_endpoints = old_endpoints_or_error.value();
  auto& new_endpoints = new_endpoints_or_error.value();
  if (old_endpoints.empty() && new_endpoints.empty()) {
    return pending_changes;
  }
  const DnsSdInstanceEndpoint& some_ep =
      !old_endpoints.empty() ? old_endpoints.front() : new_endpoints.front();
  ServiceKey key(some_ep.service_id(), some_ep.domain_id());
  auto it = callback_map_.find(key);
  if (it == callback_map_.end()) {
    return pending_changes;
  }
  const std::vector<Callback*> callbacks = it->second;

  // Calculate the changes.
  //
  // NOTE: As the input sets are expected to be small, the generated sets will
  // also be small.
  std::vector<DnsSdInstanceEndpoint> created;
  std::vector<DnsSdInstanceEndpoint> updated;
  std::vector<DnsSdInstanceEndpoint> deleted;
  CalculateChangeSets(std::move(old_endpoints), std::move(new_endpoints),
                      &created, &updated, &deleted);

  // Inform callbacks.
  for (Callback* callback : callbacks) {
    for (const DnsSdInstanceEndpoint& endpoint : created) {
      callback->OnEndpointCreated(endpoint);
    }
    for (const DnsSdInstanceEndpoint& endpoint : updated) {
      callback->OnEndpointUpdated(endpoint);
    }
    for (const DnsSdInstanceEndpoint& endpoint : deleted) {
      callback->OnEndpointDeleted(endpoint);
    }
  }

  return pending_changes;
}

ErrorOr<std::vector<PendingQueryChange>> QuerierImpl::ApplyRecordChanges(
    const MdnsRecord& record,
    RecordChangedEvent event) {
  std::vector<PendingQueryChange> pending_changes;
  std::function<void(const DomainName&)> creation_callback(
      [this, &pending_changes](const DomainName& domain) mutable {
        pending_changes.push_back({domain, DnsType::kANY, DnsClass::kANY, this,
                                   PendingQueryChange::kStartQuery});
      });
  std::function<void(const DomainName&)> deletion_callback(
      [this, &pending_changes](const DomainName& domain) mutable {
        pending_changes.push_back({domain, DnsType::kANY, DnsClass::kANY, this,
                                   PendingQueryChange::kStopQuery});
      });
  Error result =
      graph_->ApplyDataRecordChange(record, event, std::move(creation_callback),
                                    std::move(deletion_callback));
  if (!result.ok()) {
    return result;
  } else {
    return pending_changes;
  }
}

}  // namespace discovery
}  // namespace openscreen
