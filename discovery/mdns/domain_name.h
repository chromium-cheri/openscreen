// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_DOMAIN_NAME_H_
#define DISCOVERY_MDNS_DOMAIN_NAME_H_

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

namespace openscreen {
namespace mdns {

struct DomainName {
  static DomainName Append(const DomainName& first, const DomainName& second);
  static void Append(const DomainName& first,
                     const DomainName& second,
                     uint8_t* result);
  static bool FromLabels(const std::vector<std::string>& labels,
                         DomainName* result);

  DomainName();
  explicit DomainName(std::vector<uint8_t> domain_name);
  DomainName(const DomainName&);
  DomainName(DomainName&&);
  ~DomainName();
  DomainName& operator=(const DomainName&);
  DomainName& operator=(DomainName&&);

  bool operator==(const DomainName& other) const;
  bool operator!=(const DomainName& other) const;

  DomainName& Append(const DomainName& after);
  // TODO: If there's significant use of this, we would rather have string_span
  // or similar for this so we could use iterators for zero-copy.
  std::vector<std::string> GetLabels() const;

  // RFC 1034 domain name format: sequence of 1 octet label length followed by
  // label data, ending with a 0 octet.  May not exceed 256 bytes (including
  // terminating 0).
  std::vector<uint8_t> domain_name;
};

std::ostream& operator<<(std::ostream& os, const DomainName& domain_name);

}  // namespace mdns
}  // namespace openscreen

#endif
