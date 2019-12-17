// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_PUBLIC_DNS_SD_SERVICE_PUBLISHER_H_
#define DISCOVERY_PUBLIC_DNS_SD_SERVICE_PUBLISHER_H_

#include "discovery/dnssd/public/dns_sd_instance_record.h"
#include "discovery/dnssd/public/dns_sd_querier.h"
#include "discovery/dnssd/public/dns_sd_service.h"
#include "platform/base/error.h"

namespace openscreen {
namespace discovery {

// This class represents a top-level service publication API which sits on top
// of DNS-SD. T is the service-specific type which stores information regarding
// a specific service instance.
template <typename T>
class DnsSdServicePublisher {
 public:
  DnsSdServicePublisher(DnsSdService* service,
                        std::function<DnsSdInstanceRecord(const T&)> conversion,
                        std::string service_name)
      : conversion_(conversion),
        service_(service),
        service_name_(std::move(service_name)) {}

  ~DnsSdServicePublisher() = default;

  // Begins publishing the service associated with the provided service
  // information.
  void StartPublishing(const T& service_info) {
    // TODO(rwkeane): Call DnsSdPublisher::Register.
  }

  // Stops publishing the service associated with the provided service
  // information.
  void StopPublishingAll() {
    // TODO(rwkeane): Call DnsSdPublisher::DeregisterAll.
  }

 protected:
  // Converts from a type T to a DnsSdInstanceRecord which can be provided to
  // the underlying DNS-SD layer.
  virtual DnsSdInstanceRecord ToRecord(const T& instance) const = 0;

 private:
  // Converts between the external representation of a service and the internal
  // DNS-SD representation.
  std::function<DnsSdInstanceRecord(const T&)> conversion_;

  DnsSdService* const service_;

  const std::string service_name_;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_PUBLIC_DNS_SD_SERVICE_PUBLISHER_H_
