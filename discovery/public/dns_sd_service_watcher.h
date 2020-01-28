// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_PUBLIC_DNS_SD_SERVICE_WATCHER_H_
#define DISCOVERY_PUBLIC_DNS_SD_SERVICE_WATCHER_H_

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "discovery/dnssd/public/dns_sd_instance_record.h"
#include "discovery/dnssd/public/dns_sd_querier.h"
#include "discovery/dnssd/public/dns_sd_service.h"
#include "platform/base/error.h"
#include "util/logging.h"

namespace openscreen {
namespace discovery {

// This class represents a top-level discovery API which sits on top of DNS-SD.
// T is the service-specific type which stores information regarding a specific
// service instance.
template <typename T>
class DnsSdServiceWatcher : public DnsSdQuerier::Callback {
 public:
  // The method which will be called when any new service instance is
  // discovered, a service instance changes its data (such as TXT or A data), or
  // a previously discovered service instance ceases to be available. The vector
  // is the set of all currently active service instances which have been
  // discovered so far.
  using ServicesUpdatedCallback = std::function<void(std::vector<T> services)>;

  // This function type is responsible for converting from a DNS service
  // instance (received from another mDNS endpoint) to a T type to be returned
  // to the caller.
  using ServiceConverter = std::function<T(const DnsSdInstanceRecord&)>;

  DnsSdServiceWatcher(DnsSdService* service,
                      TaskRunner* task_runner,
                      std::string service_name,
                      ServiceConverter conversion,
                      ServicesUpdatedCallback callback)
      : conversion_(conversion),
        service_name_(std::move(service_name)),
        callback_(std::move(callback)),
        task_runner_(task_runner),
        querier_(service ? service->GetQuerier() : nullptr) {
    OSP_DCHECK(task_runner_);
    OSP_DCHECK(querier_);
  }

  ~DnsSdServiceWatcher() = default;

  // Starts service discovery.
  void StartDiscovery() {
    OSP_DCHECK(!is_running_);
    is_running_ = true;

    if (!task_runner_->IsRunningOnTaskRunner()) {
      task_runner_->PostTask([this]() mutable { StartDiscovery(); });
      return;
    }

    querier_->StartQuery(service_name_, this);
  }

  // Stops service discovery.
  void StopDiscovery() {
    OSP_DCHECK(is_running_);
    is_running_ = false;

    if (!task_runner_->IsRunningOnTaskRunner()) {
      task_runner_->PostTask([this]() mutable { StopDiscovery(); });
      return;
    }

    querier_->StopQuery(service_name_, this);
  }

  // Returns whether or not discovery is currently ongoing.
  bool is_running() const { return is_running_; }

  // Re-initializes the process of service discovery, even if the underlying
  // implementation would not normally do so at this time. All previously
  // received service data is discarded.
  // NOTE: This call will return an error if StartDiscovery has not yet been
  // called.
  Error ForceRefresh() {
    if (!is_running_) {
      return Error::Code::kOperationInvalid;
    }

    if (!task_runner_->IsRunningOnTaskRunner()) {
      task_runner_->PostTask([this]() mutable { ForceRefresh(); });
      return Error::None();
    }

    querier_->ReinitializeQueries(service_name_);
    records_.clear();
    return Error::None();
  }

  // Re-initializes the process of service discovery, even if the underlying
  // implementation would not normally do so at this time. All previously
  // received service data is persisted.
  // NOTE: This call will return an error if StartDiscovery has not yet been
  // called.
  Error DiscoverNow() {
    if (!is_running_) {
      return Error::Code::kOperationInvalid;
    }

    if (!task_runner_->IsRunningOnTaskRunner()) {
      task_runner_->PostTask([this]() mutable { DiscoverNow(); });
      return Error::None();
    }

    querier_->ReinitializeQueries(service_name_);
    return Error::None();
  }

  // Calls the provided |callback_| with all services which have been discovered
  // so far.
  void GetServices() const {
    if (!task_runner_->IsRunningOnTaskRunner()) {
      task_runner_->PostTask([this]() { GetServices(); });
      return;
    }

    std::vector<T> refs;
    for (const auto& pair : records_) {
      refs.push_back(pair.second);
    }
    callback_(GetServicesList());
  }

 private:
  friend class TestServiceWatcher;

  // DnsSdQuerier::Callback overrides.
  void OnInstanceCreated(const DnsSdInstanceRecord& new_record) override {
    // NOTE: existence is not checked because records may be overwritten after
    // querier_->ReinitializeQueries() is called.
    records_[new_record.instance_id()] = conversion_(new_record);
    callback_(GetServicesList());
  }

  void OnInstanceUpdated(const DnsSdInstanceRecord& modified_record) override {
    auto it = records_.find(modified_record.instance_id());
    if (it != records_.end()) {
      records_.erase(it);
      records_.emplace(modified_record.instance_id(),
                       conversion_(modified_record));

      callback_(GetServicesList());
    } else {
      OSP_LOG << "Received modified record for non-existent DNS-SD Instance "
              << modified_record.instance_id();
    }
  }

  void OnInstanceDeleted(const DnsSdInstanceRecord& old_record) override {
    if (records_.erase(old_record.instance_id())) {
      callback_(GetServicesList());
    } else {
      OSP_LOG << "Received deletion of record for non-existent DNS-SD Instance "
              << old_record.instance_id();
    }
  }

  // Returns a list of all services received so far.
  std::vector<T> GetServicesList() const {
    if (!task_runner_->IsRunningOnTaskRunner()) {
      OSP_NOTREACHED();
    }

    std::vector<T> refs;
    for (const auto& pair : records_) {
      refs.push_back(pair.second);
    }
    return refs;
  }

  // Set of all instance ids found so far, mapped to the T type that it
  // represents.
  std::unordered_map<std::string, T> records_;

  // Represents whether discovery is currently running or not.
  std::atomic_bool is_running_{false};

  // Converts from the DNS-SD representation of a service to the outside
  // representation.
  ServiceConverter conversion_;

  std::string service_name_;
  ServicesUpdatedCallback callback_;
  TaskRunner* const task_runner_;
  DnsSdQuerier* const querier_;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_PUBLIC_DNS_SD_SERVICE_WATCHER_H_
