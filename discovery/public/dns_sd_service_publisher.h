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

// This class represents a top-level discovery API which sits on top of DNS-SD.
template <typename T>
class DnsSdServicePublisher : public DnsSdQuerier::Callback {
 public:
  explicit DnsSdServicePublisher(DnsSdService* service) : service_(service) {}

  virtual ~DnsSdServicePublisher() = default;

  // Begins publishing the service associated with the provided service
  // information.
  void Start(const T& service_info) {
    // TODO(rwkeane): Call DnsSdPublisher::Register.
  }

  // Stops publishing the service associated with the provided service
  // information.
  void StopAll() {
    // TODO(rwkeane): Call DnsSdPublisher::DeregisterAll.
  }

 protected:
  // Converts from a type T to a DnsSdInstanceRecord which can be provided to
  // the underlying DNS-SD layer.
  virtual DnsSdInstanceRecord ToRecord(const T& instance) const = 0;

  // Returns the DNS-SD service ID.
  virtual const std::string& GetServiceName() const = 0;

 private:
  DnsSdService* const service_;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_PUBLIC_DNS_SD_SERVICE_PUBLISHER_H_
