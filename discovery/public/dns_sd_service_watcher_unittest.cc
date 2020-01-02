// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/public/dns_sd_service_watcher.h"

#include "discovery/dnssd/dns_sd_instance_record.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace discovery {

std::string Convert(const DnssdInstanceRecord& record) {
  return record.instance_id();
}

}  // namespace discovery
}  // namespace openscreen
