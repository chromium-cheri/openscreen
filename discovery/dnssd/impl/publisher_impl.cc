// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/publisher_impl.h"

#include <map>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "discovery/common/reporting_client.h"
#include "discovery/dnssd/impl/conversion_layer.h"
#include "discovery/dnssd/impl/instance_key.h"
#include "discovery/dnssd/impl/network_config.h"
#include "discovery/mdns/public/mdns_constants.h"
#include "platform/api/task_runner.h"
#include "platform/base/error.h"

namespace openscreen {
namespace discovery {
namespace {

DnsSdInstanceEndpoint UpdateDomain(const DomainName& domain,
                                   const DnsSdInstanceRecord& record,
                                   const NetworkConfig& network_config_) {
  InstanceKey key(domain);
  if (!network_config_.address_v4() || !network_config_.address_v6()) {
    const IPAddress& address = network_config_.address_v4()
                                   ? network_config_.address_v4()
                                   : network_config_.address_v6();
    OSP_DCHECK(address);
    IPEndpoint endpoint{address, record.port()};
    return DnsSdInstanceEndpoint(key.instance_id(), key.service_id(),
                                 key.domain_id(), record.txt(), endpoint,
                                 network_config_.network_interface());
  } else {
    IPEndpoint endpoint_v4{network_config_.address_v4(), record.port()};
    IPEndpoint endpoint_v6{network_config_.address_v6(), record.port()};
    return DnsSdInstanceEndpoint(
        key.instance_id(), key.service_id(), key.domain_id(), record.txt(),
        endpoint_v4, endpoint_v6, network_config_.network_interface());
  }
}

DnsSdInstanceEndpoint CreateEndpoint(DnsSdInstanceRecord record,
                                     const NetworkConfig& network_config_) {
  if (!network_config_.address_v4() || !network_config_.address_v6()) {
    const IPAddress& address = network_config_.address_v4()
                                   ? network_config_.address_v4()
                                   : network_config_.address_v6();
    OSP_DCHECK(address);
    return DnsSdInstanceEndpoint(std::move(record), address,
                                 network_config_.network_interface());
  } else {
    return DnsSdInstanceEndpoint(
        std::move(record), network_config_.address_v4(),
        network_config_.address_v6(), network_config_.network_interface());
  }
}

template <typename T>
inline typename std::map<DnsSdInstanceRecord, T>::iterator FindKey(
    std::map<DnsSdInstanceRecord, T>* records,
    const InstanceKey& key) {
  return std::find_if(records->begin(), records->end(),
                      [&key](const std::pair<DnsSdInstanceRecord, T>& pair) {
                        return key == InstanceKey(pair.first);
                      });
}

template <typename T>
int EraseRecordsWithServiceId(std::map<DnsSdInstanceRecord, T>* records,
                              const std::string& service_id) {
  int removed_count = 0;
  for (auto it = records->begin(); it != records->end();) {
    if (it->first.service_id() == service_id) {
      removed_count++;
      it = records->erase(it);
    } else {
      it++;
    }
  }

  return removed_count;
}

}  // namespace

PublisherImpl::PublisherImpl(MdnsService* publisher,
                             ReportingClient* reporting_client,
                             TaskRunner* task_runner,
                             const NetworkConfig& network_config)
    : mdns_publisher_(publisher),
      reporting_client_(reporting_client),
      task_runner_(task_runner),
      network_config_(network_config) {
  OSP_DCHECK(mdns_publisher_);
  OSP_DCHECK(reporting_client_);
  OSP_DCHECK(task_runner_);
}

PublisherImpl::~PublisherImpl() = default;

Error PublisherImpl::Register(const DnsSdInstanceRecord& record,
                              Client* client) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  OSP_DCHECK(client != nullptr);

  if (published_records_.find(record) != published_records_.end()) {
    UpdateRegistration(record);
  } else if (pending_records_.find(record) != pending_records_.end()) {
    return Error::Code::kOperationInProgress;
  }

  InstanceKey key(record);
  const IPAddress& address_v4 = network_config_.address_v4();
  const IPAddress& address_v6 = network_config_.address_v6();
  const IPAddress& address = address_v4 ? address_v4 : address_v6;
  OSP_DCHECK(address);
  pending_records_.emplace(CreateEndpoint(record, network_config_), client);

  OSP_DVLOG << "Registering instance '" << record.instance_id() << "'";

  return mdns_publisher_->StartProbe(this, GetDomainName(key), address);
}

Error PublisherImpl::UpdateRegistration(const DnsSdInstanceRecord& record) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  // Check if the record is still pending publication.
  auto it = FindKey(&pending_records_, InstanceKey(record));

  OSP_DVLOG << "Updating instance '" << record.instance_id() << "'";

  // If it is a pending record, update it. Else, try to update a published
  // record.
  if (it != pending_records_.end()) {
    // The instance, service, and domain ids have not changed, so only the
    // remaining data needs to change. The ongoing probe does not need to be
    // modified.
    Client* const client = it->second;
    pending_records_.erase(it);
    pending_records_.emplace(CreateEndpoint(record, network_config_), client);
    return Error::None();
  } else {
    return UpdatePublishedRegistration(record);
  }
}

Error PublisherImpl::UpdatePublishedRegistration(
    const DnsSdInstanceRecord& record) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  auto published_record_it = FindKey(&published_records_, InstanceKey(record));

  // Check preconditions called out in header. Specifically, the updated record
  // must be making changes to an already published record.
  if (published_record_it == published_records_.end()) {
    return Error::Code::kParameterInvalid;
  }

  const DnsSdInstanceEndpoint updated_record =
      UpdateDomain(GetDomainName(InstanceKey(published_record_it->second)),
                   record, network_config_);
  if (published_record_it->second == updated_record) {
    return Error::Code::kParameterInvalid;
  }

  // Get all records which have changed. By design, there an only be one record
  // of each DnsType, so use that here to simplify this step.
  // First in each pair is the old records, second is the new record.
  std::map<DnsType,
           std::pair<absl::optional<MdnsRecord>, absl::optional<MdnsRecord>>>
      changed_records;
  const std::vector<MdnsRecord> old_records =
      GetDnsRecords(published_record_it->second);
  const std::vector<MdnsRecord> new_records = GetDnsRecords(updated_record);

  // Populate the first part of each pair in |changed_records|.
  for (size_t i = 0; i < old_records.size(); i++) {
    const auto key = old_records[i].dns_type();
    OSP_DCHECK(changed_records.find(key) == changed_records.end());
    auto value = std::make_pair(std::move(old_records[i]), absl::nullopt);
    changed_records.emplace(key, std::move(value));
  }

  // Populate the second part of each pair in |changed_records|.
  for (size_t i = 0; i < new_records.size(); i++) {
    const auto key = new_records[i].dns_type();
    auto find_it = changed_records.find(key);
    if (find_it == changed_records.end()) {
      std::pair<absl::optional<MdnsRecord>, absl::optional<MdnsRecord>> value(
          absl::nullopt, std::move(new_records[i]));
      changed_records.emplace(key, std::move(value));
    } else {
      find_it->second.second = std::move(new_records[i]);
    }
  }

  // Apply changes called out in |changed_records|.
  // TODO(crbug.com/openscreen/114): Trace each below call so multiple errors
  // can be seen.
  Error total_result = Error::None();
  for (const auto& pair : changed_records) {
    OSP_DCHECK(pair.second.first != absl::nullopt ||
               pair.second.second != absl::nullopt);
    if (pair.second.first == absl::nullopt) {
      auto error = mdns_publisher_->RegisterRecord(pair.second.second.value());
      if (!error.ok()) {
        total_result = error;
      }
    } else if (pair.second.second == absl::nullopt) {
      auto error = mdns_publisher_->UnregisterRecord(pair.second.first.value());
      if (!error.ok()) {
        total_result = error;
      }
    } else if (pair.second.first.value() != pair.second.second.value()) {
      auto error = mdns_publisher_->UpdateRegisteredRecord(
          pair.second.first.value(), pair.second.second.value());
      if (!error.ok()) {
        total_result = error;
      }
    }
  }

  // Replace the old records with the new ones.
  published_records_.erase(published_record_it);
  published_records_.emplace(record, std::move(updated_record));

  return total_result;
}

ErrorOr<int> PublisherImpl::DeregisterAll(const std::string& service) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  OSP_DVLOG << "Deregistering all instances";

  int removed_count = 0;

  // TODO(crbug.com/openscreen/114): Trace each below call so multiple errors
  // can be seen.
  Error error = Error::None();
  for (auto it = published_records_.begin(); it != published_records_.end();) {
    if (it->second.service_id() == service) {
      for (const auto& mdns_record : GetDnsRecords(it->second)) {
        auto publisher_error = mdns_publisher_->UnregisterRecord(mdns_record);
        if (!publisher_error.ok()) {
          error = publisher_error;
        }
      }
      removed_count++;
      it = published_records_.erase(it);
    } else {
      it++;
    }
  }

  removed_count += EraseRecordsWithServiceId(&pending_records_, service);

  if (!error.ok()) {
    return error;
  } else {
    return removed_count;
  }
}

void PublisherImpl::OnDomainFound(const DomainName& requested_name,
                                  const DomainName& confirmed_name) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  OSP_DVLOG << "Domain successfully claimed: '" << confirmed_name.ToString()
            << "' based on requested name: '" << requested_name.ToString()
            << "'";

  auto it = FindKey(&pending_records_, InstanceKey(requested_name));

  if (it == pending_records_.end()) {
    // This will be hit if the record was deregister'd before the probe phase
    // was completed.
    return;
  }

  DnsSdInstanceRecord requested_record = std::move(it->first);
  DnsSdInstanceEndpoint publication =
      CreateEndpoint(requested_record, network_config_);
  Client* const client = it->second;
  pending_records_.erase(it);

  InstanceKey requested_key(requested_record);

  if (requested_name != confirmed_name) {
    OSP_DCHECK(HasValidDnsRecordAddress(confirmed_name));
    publication =
        UpdateDomain(confirmed_name, requested_record, network_config_);
  }

  for (const auto& mdns_record : GetDnsRecords(publication)) {
    Error result = mdns_publisher_->RegisterRecord(mdns_record);
    if (!result.ok()) {
      reporting_client_->OnRecoverableError(
          Error(Error::Code::kRecordPublicationError, result.ToString()));
    }
  }

  auto pair = published_records_.emplace(std::move(requested_record),
                                         std::move(publication));
  client->OnInstanceClaimed(pair.first->first, pair.first->second);
}

}  // namespace discovery
}  // namespace openscreen
