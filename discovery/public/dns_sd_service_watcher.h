// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_PUBLIC_DNS_SD_SERVICE_WATCHER_H_
#define DISCOVERY_PUBLIC_DNS_SD_SERVICE_WATCHER_H_

#include <memory>
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
namespace {

// NOTE: Must be inlined to avoid compilation failure for unused function when
// DLOGs are disabled.
template <typename T>
inline std::string GetInstanceNames(
    const std::unordered_map<std::string, T>& map) {
  std::string s;
  auto it = map.begin();
  if (it == map.end()) {
    return s;
  }

  s += it->first;
  while (++it != map.end()) {
    s += ", " + it->first;
  }
  return s;
}

}  // namespace

// This class represents a top-level discovery API which sits on top of DNS-SD.
// T is the service-specific type which stores information regarding a specific
// service instance.
// TODO(rwkeane): Include reporting client as ctor parameter once parallel CLs
// are in.
// NOTE: This class is not thread-safe and calls will be made to DnsSdService in
// the same sequence and on the same threads from which these methods are
// called. This is to avoid forcing design decisions on embedders who write
// their own implementations of the DNS-SD layer.
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
  using ServiceInstanceConverter =
      std::function<ErrorOr<T>(DnsSdInstanceRecord)>;

  DnsSdServiceWatcher(DnsSdService* service,
                      std::string service_name,
                      ServiceInstanceConverter conversion,
                      ServicesUpdatedCallback callback)
      : conversion_(conversion),
        service_name_(std::move(service_name)),
        callback_(std::move(callback)),
        querier_(service ? service->GetQuerier() : nullptr) {
    OSP_DCHECK(querier_);
  }

  ~DnsSdServiceWatcher() = default;

  // Starts service discovery.
  void StartDiscovery() {
    OSP_DCHECK(!is_running_);
    is_running_ = true;

    querier_->StartQuery(service_name_, this);
  }

  // Stops service discovery.
  void StopDiscovery() {
    OSP_DCHECK(is_running_);
    is_running_ = false;

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

    querier_->ReinitializeQueries(service_name_);
    return Error::None();
  }

  // Returns the set of services which have been received so far.
  std::vector<T> GetServices() const {
    std::vector<T> results;
    for (const auto& pair : records_) {
      results.push_back(pair.second);
    }

    OSP_DVLOG << "Currently " << records_.size()
              << " known service instances: [" << GetInstanceNames(records_)
              << "]";

    return results;
  }

 private:
  friend class TestServiceWatcher;

  // DnsSdQuerier::Callback overrides.
  void OnInstanceCreated(DnsSdInstanceRecord new_record) override {
    // NOTE: existence is not checked because records may be overwritten after
    // querier_->ReinitializeQueries() is called.
    std::string instance_id = new_record.instance_id();
    ErrorOr<T> record = conversion_(std::move(new_record));
    if (record.is_error()) {
      OSP_LOG << "Conversion of received record failed with error: "
              << record.error();
      return;
    }
    records_[std::move(instance_id)] = std::move(record.value());
    callback_(GetServices());
  }

  void OnInstanceUpdated(DnsSdInstanceRecord modified_record) override {
    auto it = records_.find(modified_record.instance_id());
    if (it == records_.end()) {
      OSP_LOG << "Received modified record for non-existent DNS-SD Instance "
              << modified_record.instance_id();
      return;
    }

    ErrorOr<T> record = conversion_(std::move(modified_record));
    if (record.is_error()) {
      OSP_LOG << "Conversion of received record failed with error: "
              << record.error();
      return;
    }

    it->second = std::move(record.value());

    callback_(GetServices());
  }

  void OnInstanceDeleted(DnsSdInstanceRecord old_record) override {
    if (records_.erase(old_record.instance_id())) {
      callback_(GetServices());
    } else {
      OSP_LOG << "Received deletion of record for non-existent DNS-SD Instance "
              << old_record.instance_id();
    }
  }

  // Set of all instance ids found so far, mapped to the T type that it
  // represents. NOTE: Unordered map is used because this set is in many cases
  // expected to be large.
  std::unordered_map<std::string, T> records_;

  // Represents whether discovery is currently running or not.
  bool is_running_ = false;

  // Converts from the DNS-SD representation of a service to the outside
  // representation.
  ServiceInstanceConverter conversion_;

  std::string service_name_;
  ServicesUpdatedCallback callback_;
  DnsSdQuerier* const querier_;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_PUBLIC_DNS_SD_SERVICE_WATCHER_H_
