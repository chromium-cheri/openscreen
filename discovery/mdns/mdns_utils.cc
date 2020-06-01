// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_utils.h"

#include <sstream>
#include <vector>

namespace openscreen {
namespace discovery {

std::string GetRecordLog(const MdnsRecord& record) {
  std::stringstream ss;
  ss << "name: '" << record.name().ToString() << "'";
  ss << ", type: " << record.dns_type();

  if (record.dns_type() == DnsType::kPTR) {
    const DomainName& target =
        absl::get<PtrRecordRdata>(record.rdata()).ptr_domain();
    ss << ", target: '" << target.ToString() << "'";
  } else if (record.dns_type() == DnsType::kSRV) {
    const DomainName& target =
        absl::get<SrvRecordRdata>(record.rdata()).target();
    ss << ", target: '" << target.ToString() << "'";
  } else if (record.dns_type() == DnsType::kNSEC) {
    const auto& nsec_rdata = absl::get<NsecRecordRdata>(record.rdata());
    std::vector<DnsType> types = nsec_rdata.types();
    ss << ", representing [";
    if (!types.empty()) {
      auto it = types.begin();
      ss << *it++;
      while (it != types.end()) {
        ss << ", " << *it++;
      }
      ss << "]";
    }
  }

  return ss.str();
}

}  // namespace discovery
}  // namespace openscreen
