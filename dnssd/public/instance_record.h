// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DNSSD_PUBLIC_INSTANCE_RECORD_H_
#define DNSSD_PUBLIC_INSTANCE_RECORD_H_

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "dnssd/public/txt_record.h"
#include "platform/base/ip_address.h"

namespace openscreen {
namespace dnssd {

// Represents the data stored in DNS records of types SRV, TXT, A, and AAAA
class InstanceRecord {
 public:
  // Returns the instance name for this DNS-SD record.
  const std::string& instance_id() const;

  // Returns the service id for this DNS-SD record.
  const std::string& service_id() const;

  // Returns the domain id for this DNS-SD record.
  const std::string& domain_id() const;

  // Returns the addess associated with this DNS-SD record. In any valid record,
  // at least one of these properties should not be absl::nullopt.
  const absl::optional<IPEndpoint>& address_v4() const;
  const absl::optional<IPEndpoint>& address_v6() const;

  // Returns the TXT record associated with this DNS-SD record
  const TxtRecord& txt() const;
};

}  // namespace dnssd
}  // namespace openscreen

#endif  // DNSSD_PUBLIC_INSTANCE_RECORD_H_
