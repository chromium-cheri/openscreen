// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/querier_impl.h"

#include <string>
#include <vector>

#include "platform/api/task_runner.h"
#include "util/logging.h"

namespace openscreen {
namespace discovery {
namespace {

static constexpr char kLocalDomain[] = "local";

}  // namespace

QuerierImpl::QuerierImpl(MdnsService* mdns_querier, TaskRunner* task_runner)
    : mdns_querier_(mdns_querier), task_runner_(task_runner) {
  OSP_DCHECK(mdns_querier_);
  OSP_DCHECK(task_runner_);
}

QuerierImpl::~QuerierImpl() = default;

void QuerierImpl::StartQuery(const std::string& service, Callback* callback) {
  OSP_DCHECK(callback);
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  ServiceKey key(service, kLocalDomain);

  if (!IsQueryRunning(key)) {
    callback_map_[key] = {callback};
    StartDnsQuery(std::move(key));
  } else {
    callback_map_[key].push_back(callback);

    for (auto it = received_records_.begin(); it != received_records_.end();
         it++) {
      if (it->first == key) {
        auto received_it = received_records_.find(it->first);
        if (received_it == received_records_.end()) {
          continue;
        }

        ErrorOr<DnsSdInstanceRecord> record =
            received_it->second.CreateRecord();
        if (record.is_value()) {
          callback->OnInstanceCreated(record.value());
        }
      }
    }
  }
}

bool QuerierImpl::IsQueryRunning(const std::string& service) const {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  return IsQueryRunning(ServiceKey(service, kLocalDomain));
}

void QuerierImpl::StopQuery(const std::string& service, Callback* callback) {
  OSP_DCHECK(callback);
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  ServiceKey key(service, kLocalDomain);
  auto callback_it = callback_map_.find(key);
  if (callback_it == callback_map_.end()) {
    return;
  }
  std::vector<Callback*>* callbacks = &callback_it->second;

  const auto it = std::find(callbacks->begin(), callbacks->end(), callback);
  if (it != callbacks->end()) {
    callbacks->erase(it);
    if (callbacks->empty()) {
      callback_map_.erase(callback_it);
      StopDnsQuery(std::move(key));
    }
  }
}

void QuerierImpl::ReinitializeQueries(const std::string& service) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  // Stop instance-specific queries and erase all instance data received so far.
  for (auto it = received_records_.begin(); it != received_records_.end();) {
    if (it->first == key) {
      it = received_records_.erase(it);
    } else {
      it++;
    }
  }

  auto instance_queries_it = ongoing_instance_queries_.find(key);
  if (instance_queries_it != ongoing_instance_queries_.end()) {
    for (const InstanceKey& key : instance_queries_it->second) {
      const auto query = GetInstanceQueryInfo(key);
      mdns_querier_->StopQuery(query.name, query.dns_type, query.dns_class,
                               this);
    }
  }

  // Restart top-level queries.
  const ServiceKey key(service, kLocalDomain);
  mdns_querier_->ReinitializeQueries(GetPtrQueryInfo(key).name);
}

void QuerierImpl::OnRecordChanged(const MdnsRecord& record,
                                  RecordChangedEvent event) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  IsPtrRecord(record) ? HandlePtrRecordChange(record, event)
                      : HandleNonPtrRecordChange(record, event);
}

Error QuerierImpl::HandlePtrRecordChange(const MdnsRecord& record,
                                         RecordChangedEvent event) {
  if (!HasValidDnsRecordAddress(record)) {
    // This means that the received record is malformed.
    return Error::Code::kParameterInvalid;
  }

  InstanceKey key(record);
  switch (event) {
    case RecordChangedEvent::kCreated:
      StartDnsQuery(std::move(key));
      return Error::None();
    case RecordChangedEvent::kExpired:
      StopDnsQuery(std::move(key));
      return Error::None();
    case RecordChangedEvent::kUpdated:
      return Error::Code::kOperationInvalid;
  }
  return Error::Code::kUnknownError;
}

Error QuerierImpl::HandleNonPtrRecordChange(const MdnsRecord& record,
                                            RecordChangedEvent event) {
  if (!HasValidDnsRecordAddress(record)) {
    // This means that the call received had malformed data.
    return Error::Code::kParameterInvalid;
  }

  const ServiceKey key(record);
  if (!IsQueryRunning(key)) {
    // This means that the call was already queued up on the TaskRunner when the
    // callback was removed. The caller no longer cares, so drop the record.
    return Error::Code::kOperationCancelled;
  }
  const std::vector<Callback*>& callbacks = callback_map_[key];

  // Get the current InstanceRecord data associated with the received record.
  const InstanceKey id(record);
  ErrorOr<DnsSdInstanceRecord> old_instance_record = Error::Code::kItemNotFound;
  auto it = received_records_.find(id);
  if (it == received_records_.end()) {
    it = received_records_.emplace(id, DnsData(id)).first;
  } else {
    old_instance_record = it->second.CreateRecord();
  }
  DnsData* data = &it->second;

  // Apply the changes specified by the received event to the stored
  // InstanceRecord.
  Error apply_result = data->ApplyDataRecordChange(record, event);
  if (!apply_result.ok()) {
    OSP_LOG_ERROR << "Received erroneous record change. Error: "
                  << apply_result;
    return apply_result;
  }

  // Send an update to the user, based on how the new and old records compare.
  ErrorOr<DnsSdInstanceRecord> new_instance_record = data->CreateRecord();
  NotifyCallbacks(callbacks, old_instance_record, new_instance_record);

  return Error::None();
}

void QuerierImpl::NotifyCallbacks(
    const std::vector<Callback*>& callbacks,
    const ErrorOr<DnsSdInstanceRecord>& old_record,
    const ErrorOr<DnsSdInstanceRecord>& new_record) {
  if (old_record.is_value() && new_record.is_value()) {
    for (Callback* callback : callbacks) {
      callback->OnInstanceUpdated(new_record.value());
    }
  } else if (old_record.is_value() && !new_record.is_value()) {
    for (Callback* callback : callbacks) {
      callback->OnInstanceDeleted(old_record.value());
    }
  } else if (!old_record.is_value() && new_record.is_value()) {
    for (Callback* callback : callbacks) {
      callback->OnInstanceCreated(new_record.value());
    }
  }
}

void QuerierImpl::EraseInstancesOf(InstanceKey key) {
  auto record_it = received_records_.find(key);
  if (record_it == received_records_.end()) {
    return;
  }

  ErrorOr<DnsSdInstanceRecord> instance_record =
      record_it->second.CreateRecord();
  if (instance_record.is_value()) {
    const auto it = callback_map_.find(key);
    if (it != callback_map_.end()) {
      for (Callback* callback : it->second) {
        callback->OnInstanceDeleted(instance_record.value());
      }
    }
  }

  received_records_.erase(record_it);
}

void QuerierImpl::StartDnsQuery(InstanceKey key) {
  // An InstanceKey is a ServiceKey, so it can be used as the key for this map.
  auto pair =
      ongoing_instance_queries_.emplace(key, std::vector<InstanceKey>{});
  if (!pair.second) {
    auto key_it =
        std::find(pair.first->second.begin(), pair.first->second.end(), key);
    if (key_it != pair.first->second.end()) {
      return;
    }
  }

  pair.first->second.push_back(key);
  DnsQueryInfo query = GetInstanceQueryInfo(key);
  mdns_querier_->StartQuery(query.name, query.dns_type, query.dns_class, this);
}

void QuerierImpl::StopDnsQuery(InstanceKey key) {
  EraseInstancesOf(key);

  // An InstanceKey is a ServiceKey, so it can be used as the key for this map.
  auto it = ongoing_instance_queries_.find(key);
  if (it == ongoing_instance_queries_.end()) {
    // This means the (service id, domain id) pair isn't associated with any
    // instance keys.
    return;
  }
  auto key_it = std::find(it->second.begin(), it->second.end(), key);
  if (key_it == it->second.end()) {
    // This means the InstanceKey being checked for doesn't exist.
    return;
  }

  // Erase the key and if there are no other keys left, erase the whole map
  // entry.
  it->second.erase(key_it);
  if (it->second.empty()) {
    ongoing_instance_queries_.erase(it);
  }

  // Call to the mDNS layer to stop the query.
  DnsQueryInfo query = GetInstanceQueryInfo(key);
  mdns_querier_->StopQuery(query.name, query.dns_type, query.dns_class, this);
}

void QuerierImpl::StartDnsQuery(ServiceKey key) {
  DnsQueryInfo query = GetPtrQueryInfo(key);
  mdns_querier_->StartQuery(query.name, query.dns_type, query.dns_class, this);
}

void QuerierImpl::StopDnsQuery(ServiceKey key) {
  DnsQueryInfo query = GetPtrQueryInfo(key);
  mdns_querier_->StopQuery(query.name, query.dns_type, query.dns_class, this);

  // Stop any ongoing instance-specific queries.
  auto it = ongoing_instance_queries_.find(key);
  if (it != ongoing_instance_queries_.end()) {
    for (InstanceKey instance : it->second) {
      StopDnsQuery(std::move(instance));
    }
  }
}

}  // namespace discovery
}  // namespace openscreen
