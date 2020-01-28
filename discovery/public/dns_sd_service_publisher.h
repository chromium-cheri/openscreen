// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_PUBLIC_DNS_SD_SERVICE_PUBLISHER_H_
#define DISCOVERY_PUBLIC_DNS_SD_SERVICE_PUBLISHER_H_

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
class DnsSdServicePublisher {
 public:
  // This function type is responsible for converting from a T type to a DNS
  // service instance to be published via mDNS.
  using ServiceConverter = std::function<DnsSdInstanceRecord(const T&)>;

  DnsSdServicePublisher(DnsSdService* service,
                        TaskRunner* task_runner,
                        ServiceConverter conversion,
                        const std::string& service_id)
      : conversion_(conversion),
        service_id_(service_id),
        task_runner_(task_runner),
        publisher_(service ? service->GetPublisher() : nullptr) {
    OSP_DCHECK(task_runner_);
    OSP_DCHECK(publisher_);
  }

  ~DnsSdServicePublisher() = default;

  void Register(const T& record) {
    if (!task_runner_->IsRunningOnTaskRunner()) {
      task_runner_->PostTask([this, record]() { Register(record); });
      return;
    }

    // TODO(rwkeane): Report errors through the reporting API.
    publisher_->Register(conversion_(record));
  }

  void UpdateRegistration(const T& record) {
    if (!task_runner_->IsRunningOnTaskRunner()) {
      task_runner_->PostTask([this, record]() { UpdateRegistration(record); });
      return;
    }

    // TODO(rwkeane): Report errors through the reporting API.
    publisher_->UpdateRegistration(conversion_(record));
  }

  void DeregisterAll() {
    if (!task_runner_->IsRunningOnTaskRunner()) {
      task_runner_->PostTask([this]() { DeregisterAll(); });
      return;
    }

    publisher_->DeregisterAll(service_id_);
  }

 private:
  ServiceConverter conversion_;
  const std::string service_id_;
  TaskRunner* const task_runner_;
  DnsSdPublisher* const publisher_;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_PUBLIC_DNS_SD_SERVICE_PUBLISHER_H_
