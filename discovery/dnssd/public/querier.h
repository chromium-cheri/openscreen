// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_PUBLIC_QUERIER_H_
#define DISCOVERY_DNSSD_PUBLIC_QUERIER_H_

#include "absl/strings/string_view.h"
#include "discovery/dnssd/public/instance_record.h"

namespace openscreen {
namespace discovery {
namespace dnssd {

class Querier {
 public:
  // TODO(rwkeane): Add support for expiring records in addition to deleting
  // them.
  class Callback {
   public:
    virtual ~Callback() = default;

    // Callback fired when a new InstanceRecord is created.
    virtual void OnInstanceCreated(const InstanceRecord& new_record) = 0;

    // Callback fired when an existing InstanceRecord is updated.
    virtual void OnInstanceUpdated(const InstanceRecord& modified_record) = 0;

    // Callback fired when an existing InstanceRecord is deleted.
    virtual void OnInstanceDeleted(const InstanceRecord& old_record) = 0;
  };

  virtual ~Querier() = default;

  // Begins a new query. The provided callback will be called whenever new
  // information about the provided (service, domain) pair becomes available.
  // The Callback provided is expected to persist until the StopQuery method is
  // called or this instance is destroyed.
  virtual void StartQuery(const absl::string_view& service,
                          const absl::string_view& domain,
                          Callback* cb) = 0;

  // Stops an already running query.
  virtual void StopQuery(const absl::string_view& service,
                         const absl::string_view& domain,
                         Callback* cb) = 0;
};

}  // namespace dnssd
}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_PUBLIC_QUERIER_H_
