// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/querier_impl.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "discovery/common/reporting_client.h"
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

// Removes all error instances from the below records, and calls the log
// function on all errors present in |new_endpoints|. Input vectors are expected
// to be sorted in ascending order.
void ProcessErrors(std::vector<ErrorOr<DnsSdInstanceEndpoint>>* old_endpoints,
                   std::vector<ErrorOr<DnsSdInstanceEndpoint>>* new_endpoints,
                   std::function<void(Error)> log) {
  OSP_DCHECK(old_endpoints);
  OSP_DCHECK(new_endpoints);

  auto old_it = old_endpoints->begin();
  auto new_it = new_endpoints->begin();

  // Iterate across both vectors and log new errors in the process.
  while (old_it != old_endpoints->end() && new_it != new_endpoints->end()) {
    ErrorOr<DnsSdInstanceEndpoint>& old_ep = *old_it;
    ErrorOr<DnsSdInstanceEndpoint>& new_ep = *new_it;

    if (new_ep.is_value()) {
      break;
    }

    // If they are equal, no further processing needed.
    if (old_ep == new_ep) {
      old_it++;
      new_it++;
      continue;
    }

    // Skip
    if (old_ep < new_ep) {
      old_it++;
      continue;
    }

    log(std::move(new_ep.error()));
    new_it++;
  }

  // Skip all remaining errors in the old vector.
  for (; old_it != old_endpoints->end() && old_it->is_error(); old_it++) {
  }

  // Log all errors remaining in the new vector.
  for (; new_it != new_endpoints->end() && new_it->is_error(); new_it++) {
    log(std::move(new_it->error()));
  }

  // Erase errors.
  old_endpoints->erase(old_endpoints->begin(), old_it);
  new_endpoints->erase(new_endpoints->begin(), new_it);
}

// Returns a vector containing the value of each ErrorOr<> instance provided.
// All ErrorOr<> values are expected to be non-errors.
std::vector<DnsSdInstanceEndpoint> GetValues(
    std::vector<ErrorOr<DnsSdInstanceEndpoint>> endpoints) {
  std::vector<DnsSdInstanceEndpoint> results;
  results.reserve(endpoints.size());
  for (ErrorOr<DnsSdInstanceEndpoint>& endpoint : endpoints) {
    OSP_CHECK(endpoint.is_value());
    results.push_back(std::move(endpoint.value()));
  }
  return results;
}

// Calculates the created, updated, and deleted elements using the provided
// sets, appending these values to the provided vectors. Each of the input
// vectors is expected to contain only elements such that
// |element|.is_error() == false. Additionally, input vectors are expected to
// be sorted in ascending order.
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
                         ReportingClient* reporting_client,
                         const NetworkInterfaceConfig* network_config)
    : mdns_querier_(mdns_querier),
      task_runner_(task_runner),
      reporting_client_(reporting_client),
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
    std::vector<ErrorOr<DnsSdInstanceEndpoint>> endpoints =
        graph_->CreateEndpoints(DnsDataGraph::DomainGroup::kPtr, domain);
    for (const auto& endpoint : endpoints) {
      if (endpoint.is_value()) {
        callback->OnEndpointCreated(endpoint.value());
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

  std::function<void(Error)> log = [this](Error error) mutable {
    reporting_client_->OnRecoverableError(
        Error(Error::Code::kProcessReceivedRecordFailure));
  };

  // Get the details to use for calling CreateEndpoints(). Special case PTR
  // records to optimize performance.
  const DomainName& create_endpoints_domain =
      record.dns_type() != DnsType::kPTR
          ? record.name()
          : absl::get<PtrRecordRdata>(record.rdata()).ptr_domain();
  const DnsDataGraph::DomainGroup create_endpoints_group =
      record.dns_type() != DnsType::kPTR
          ? DnsDataGraph::GetDomainGroup(record)
          : DnsDataGraph::DomainGroup::kSrvAndTxt;

  // Get the current set of DnsSdInstanceEndpoints prior to this change. Special
  // case PTR records to avoid iterating over unrelated child domains.
  std::vector<ErrorOr<DnsSdInstanceEndpoint>> old_endpoints_or_errors =
      graph_->CreateEndpoints(create_endpoints_group, create_endpoints_domain);

  // Apply the changes, creating a list of all pending changes that should be
  // applied afterwards.
  ErrorOr<std::vector<PendingQueryChange>> pending_changes_or_error =
      ApplyRecordChanges(record, event);
  if (pending_changes_or_error.is_error()) {
    OSP_DVLOG << "Failed to apply changes for " << record.dns_type()
              << " record change of type " << event << " with error "
              << pending_changes_or_error.error();
    log(std::move(pending_changes_or_error.error()));
    return {};
  }
  std::vector<PendingQueryChange>& pending_changes =
      pending_changes_or_error.value();

  // Get the new set of DnsSdInstanceEndpoints following this change.
  std::vector<ErrorOr<DnsSdInstanceEndpoint>> new_endpoints_or_errors =
      graph_->CreateEndpoints(create_endpoints_group, create_endpoints_domain);

  // Get all callbacks. If there are none, no further processing is needed. This
  // will usually be the case.
  if (old_endpoints_or_errors.empty() && new_endpoints_or_errors.empty()) {
    return pending_changes;
  }

  // Return early if the resulting sets are equal.
  std::sort(old_endpoints_or_errors.begin(), old_endpoints_or_errors.end());
  std::sort(new_endpoints_or_errors.begin(), new_endpoints_or_errors.end());
  if (std::equal(old_endpoints_or_errors.begin(), old_endpoints_or_errors.end(),
                 new_endpoints_or_errors.begin())) {
    return pending_changes;
  }

  // Log all errors and erase them.
  ProcessErrors(&old_endpoints_or_errors, &new_endpoints_or_errors,
                std::move(log));
  std::vector<DnsSdInstanceEndpoint> old_endpoints =
      GetValues(std::move(old_endpoints_or_errors));
  std::vector<DnsSdInstanceEndpoint> new_endpoints =
      GetValues(std::move(new_endpoints_or_errors));

  // Calculate the changes and inform callbacks.
  //
  // NOTE: As the input sets are expected to be small, the generated sets will
  // also be small.
  std::vector<DnsSdInstanceEndpoint> created;
  std::vector<DnsSdInstanceEndpoint> updated;
  std::vector<DnsSdInstanceEndpoint> deleted;
  CalculateChangeSets(std::move(old_endpoints), std::move(new_endpoints),
                      &created, &updated, &deleted);

  InformCallbacks(std::move(created), std::move(updated), std::move(deleted));
  return pending_changes;
}

void QuerierImpl::InformCallbacks(std::vector<DnsSdInstanceEndpoint> created,
                                  std::vector<DnsSdInstanceEndpoint> updated,
                                  std::vector<DnsSdInstanceEndpoint> deleted) {
  // Find an endpoint and use it to create the key, or return if there is none.
  DnsSdInstanceEndpoint* some_endpoint;
  if (!created.empty()) {
    some_endpoint = &created.front();
  } else if (!updated.empty()) {
    some_endpoint = &updated.front();
  } else if (!deleted.empty()) {
    some_endpoint = &deleted.front();
  } else {
    return;
  }
  ServiceKey key(some_endpoint->service_id(), some_endpoint->domain_id());

  // Find all callbacks.
  auto it = callback_map_.find(key);
  if (it == callback_map_.end()) {
    return;
  }

  // Inform callbacks.
  const std::vector<Callback*> callbacks = it->second;
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
