// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/public/instance_record.h"

#include "absl/strings/ascii.h"
#include "platform/api/logging.h"

namespace openscreen {
namespace discovery {
namespace {

bool IsValidUtf8(const std::string& string) {
  for (auto it = string.begin(); it != string.end(); it++) {
    if ((*it & 0x80) == 0x0) {  // 0xxxxxxx
      continue;
    } else if (*it >> 5 == 0x06) {  // 110xxxxx 10xxxxxx
      if (*++it >> 6 != 0x02) {
        return false;
      }
    } else if (*it >> 4 == 0x0E) {  // 1110xxxx 10xxxxxx 10xxxxxx
      if (*++it >> 6 != 0x02 || *++it >> 6 != 0x02) {
        return false;
      }
    } else if (*it >> 3 == 0x1E) {  // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
      if (*++it >> 6 != 0x02 || *++it >> 6 != 0x02 || *++it >> 6 != 0x02) {
        return false;
      }
    } else {
      return false;
    }
  }
  return true;
}

bool IsNonControlCharacters(const std::string& string) {
  for (auto string_char : string) {
    if ((string_char >= 0x0 &&
         string_char <= 0x1F /* Ascii control characters */) ||
        string_char == 0x7F /* DEL character */) {
      return false;
    }
  }
  return true;
}

}  // namespace

DnsSdInstanceRecord::DnsSdInstanceRecord(std::string instance_id,
                                         std::string service_id,
                                         std::string domain_id,
                                         IPEndpoint endpoint,
                                         DnsSdTxtRecord txt)
    : DnsSdInstanceRecord(instance_id, service_id, domain_id, txt) {
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
                                         IPEndpoint ipv4_endpoint,
                                         IPEndpoint ipv6_endpoint,
                                         DnsSdTxtRecord txt)
    : DnsSdInstanceRecord(instance_id, service_id, domain_id, txt) {
  OSP_CHECK(ipv4_endpoint.address.IsV4());
  OSP_CHECK(ipv6_endpoint.address.IsV6());

  address_v4_ = ipv4_endpoint;
  address_v6_ = ipv6_endpoint;
}

DnsSdInstanceRecord::DnsSdInstanceRecord(std::string instance_id,
                                         std::string service_id,
                                         std::string domain_id,
                                         DnsSdTxtRecord txt)
    : instance_id_(instance_id),
      service_id_(service_id),
      domain_id_(domain_id),
      txt_(txt) {
  OSP_DCHECK(IsInstanceValid(instance_id_));
  OSP_DCHECK(IsServiceValid(service_id_));
  OSP_DCHECK(IsDomainValid(domain_id_));
}

// static
bool IsInstanceValid(const std::string& instance) {
  // According to RFC6763, Instance names must:
  // - Be encoded in Net-Unicode (which required UTF-8 formatting).
  // - NOT contain ASCII control characters
  // - Be no longer than 63 octets.

  return instance.size() <= 63 && IsNonControlCharacters(instance) &&
         IsValidUtf8(instance);
}

// static
bool IsServiceValid(const std::string& service) {
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

  const std::string protocol = service.substr(service.size() - 5);
  if (protocol != "._udp" && protocol != "._tcp") {
    return false;
  }

  if (service[0] != '_' || service[1] == '-' ||
      service[service.size() - 6] == '-') {
    return false;
  }

  bool last_char_hyphen = false;
  bool seen_letter = false;
  for (size_t i = 1; i < service.size() - 5; i++) {
    if (service[i] == '-') {
      if (last_char_hyphen) {
        return false;
      }
      last_char_hyphen = true;
    } else if (absl::ascii_isalpha(service[i])) {
      seen_letter = true;
    } else if (!absl::ascii_isdigit(service[i])) {
      return false;
    }
  }

  return seen_letter;
}

// static
bool IsDomainValid(const std::string& domain) {
  // According to RFC6763 Section 4.1.3, "Because Service Instance  Names are
  // not host names, they are not constrained by the usual rules for host names
  // [...], and rich-text service subdomains are allowed and encouraged."
  // As no validation criterion is described by the RFC, and it specifies that
  // prior RFC validations do not apply, any domain should be considered valid.
  // In liu of this, validations will be used to ensure that the domain:
  // -All labels must be no longer than 63 characters
  // -Total length must be no more than 256 characters
  // -Must be encoded using valid UTF8
  // -Must not include any ASCII control characters

  if (domain.size() > 256) {
    return false;
  }

  size_t section_start = 0;
  for (size_t next_dot = domain.find('.'); next_dot != std::string::npos;
       next_dot = domain.find('.', section_start)) {
    if (next_dot - section_start > 63) {
      return false;
    }
    section_start = next_dot + 1;
  }

  return IsNonControlCharacters(domain) && IsValidUtf8(domain);
}

}  // namespace discovery
}  // namespace openscreen
