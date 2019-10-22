// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/public/instance_record.h"

#include <regex>

#include "platform/api/logging.h"

namespace openscreen {
namespace discovery {

DnsSdInstanceRecord::DnsSdInstanceRecord(std::string instance_id,
                                         std::string service_id,
                                         std::string domain_id,
                                         IPEndpoint endpoint,
                                         DnsSdTxtRecord txt)
    : instance_id_(instance_id),
      service_id_(service_id),
      domain_id_(domain_id),
      txt_(txt) {
  OSP_DCHECK(IsInstanceValid(instance_id_));
  OSP_DCHECK(IsServiceValid(service_id_));
  OSP_DCHECK(IsDomainValid(domain_id_));

  if (endpoint.address.IsV4()) {
    address_v4_ = endpoint;
  } else if (endpoint.address.IsV6()) {
    address_v6_ = endpoint;
  } else {
    OSP_NOTREACHED();
  }
}

DnsSdInstanceRecord::DnsSdInstanceRecord(std::string instance_id,
                                         std::string service_id,
                                         std::string domain_id,
                                         IPEndpoint endpoint1,
                                         IPEndpoint endpoint2,
                                         DnsSdTxtRecord txt)
    : instance_id_(instance_id),
      service_id_(service_id),
      domain_id_(domain_id),
      txt_(txt) {
  OSP_DCHECK(IsInstanceValid(instance_id_));
  OSP_DCHECK(IsServiceValid(service_id_));
  OSP_DCHECK(IsDomainValid(domain_id_));

  if (endpoint1.address.IsV4()) {
    OSP_DCHECK(endpoint2.address.IsV6());
    address_v4_ = endpoint1;
    address_v6_ = endpoint2;
  } else if (endpoint2.address.IsV4()) {
    OSP_DCHECK(endpoint1.address.IsV6());
    address_v4_ = endpoint2;
    address_v6_ = endpoint1;
  } else {
    OSP_NOTREACHED();
  }
}

// static
bool DnsSdInstanceRecord::IsInstanceValid(const std::string& instance) {
  // According to RFC6763, Instance names must:
  // - Be encoded in Net-Unicode (which required UTF-8 formatting).
  // - NOT contain ASCII control characters
  // - Be no longer than 63 octets.
  if (instance.size() > 63) {
    return false;
  }

  for (auto instance_char : instance) {
    if ((instance_char >= 0 && instance_char <= 0x1F) ||
        instance_char == 0x7F) {
      return false;
    }
  }

  // TODO(rwkeane): Validate unicode.

  return true;
}

// static
bool DnsSdInstanceRecord::IsServiceValid(const std::string& service) {
  // According to RFC6763, the service name "consists of a pair of DNS labels".
  // "The first label of the pair is an underscore character followed by the
  // Service Name" and "The second label is either '_tcp' [...] or '_udp'".
  // According to RFC6335 Section 5.1, the Service Name section must:
  //   Contain from 1 to 15 characters.
  // - Only contain A-Z, a-Z, 0-9, and the hyphen character.
  // - Contain at least one letter.
  // - NOT begin or end with a hyphen.
  // - NOT contain two adjacent hyphens.
  if (service.size() > 21 || service.size() < 7) {  // Service name size + 6.
    return false;
  }

  std::regex r(
      "_([0-9](-?[0-9])*-?)?[a-zA-Z](-?[a-zA-Z0-9])*[.]_((udp)|(tcp))");
  return regex_match(service, r);
}

// static
bool DnsSdInstanceRecord::IsDomainValid(const std::string& domain) {
  // According to RFC6763 Section 4.1.3, "Because Service Instance  Names are
  // not host names, they are not constrained by the usual rules for host names
  // [...], and rich-text service subdomains are allowed and encouraged."
  // As no validation criterion is described by the RFC, and it specifies that
  // prior RFC validations do not apply, any domain should be considered valid.
  return true;
}

}  // namespace discovery
}  // namespace openscreen
