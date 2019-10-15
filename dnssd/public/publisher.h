// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DNSSD_PUBLIC_PUBLISHER_H_
#define DNSSD_PUBLIC_PUBLISHER_H_

#include "absl/strings/string_view.h"
#include "dnssd/public/instance_record.h"

namespace dnssd {

class Publisher {
 public:
  virtual ~Publisher() = default;

  // Publishes the PTR, SRV, TXT, A, and AAAA records provided in the
  // DnsSdInstanceRecord.
  void Publish(const InstanceRecord& record);

  // Unpublishes the PTR, SRV, TXT, A, and AAAA records associated with this
  // (service, domain) pair.
  void UnpublishAll(const absl::string_view& service,
                    const absl::string_view& domain);
};

}  // namespace dnssd

#endif  // DNSSD_PUBLIC_PUBLISHER_H_
