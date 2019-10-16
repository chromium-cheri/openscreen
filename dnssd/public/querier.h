// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DNSSD_PUBLIC_QUERIER_H_
#define DNSSD_PUBLIC_QUERIER_H_

#include "absl/strings/string_view.h"
#include "dnssd/public/instance_record.h"

namespace openscreen {
namespace dnssd {

class Querier {
 public:
  class Callback {
   public:
    // Represent the ways that a DNS-SD record can change.
    enum class RecordChangeType {
      kCreated,
      kUpdated,
      kDeleted,
    };

    virtual ~Callback() = default;

    // Callback fired when a DNS-SD record changes.
    virtual void OnRecordChanged(const InstanceRecord& record,
                                 RecordChangeType type) = 0;
  };

  virtual ~Querier() = default;

  // Begins a new query. The provided callback will be called whenever new
  // information about the provided (service, domain) pair becomes available.
  virtual void StartQuery(const absl::string_view& service,
                          const absl::string_view& domain,
                          Callback* cb) = 0;

  // Stops an already running query.
  virtual void StopQuery(const absl::string_view& service,
                         const absl::string_view& domain,
                         Callback* cb) = 0;
};

}  // namespace dnssd
}  // namespace openscreen

#endif  // DNSSD_PUBLIC_QUERIER_H_
