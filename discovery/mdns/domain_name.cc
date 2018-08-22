// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/domain_name.h"

#include <algorithm>
#include <iterator>

namespace openscreen {
namespace mdns {

// static
bool DomainName::Append(const DomainName& first,
                        const DomainName& second,
                        DomainName* result) {
  DCHECK(first.domain_name_.size());
  DCHECK_EQ(first.domain_name_.back(), 0);
  DCHECK(second.domain_name_.size());
  DCHECK_EQ(second.domain_name_.back(), 0);
  if ((first.domain_name_.size() + second.domain_name_.size() - 1) >
      kDomainNameMaxLength) {
    return false;
  }
  result->domain_name_.clear();
  result->domain_name_.insert(result->domain_name_.begin(),
                              first.domain_name_.begin(),
                              first.domain_name_.end());
  result->domain_name_.insert(result->domain_name_.end() - 1,
                              second.domain_name_.begin(),
                              second.domain_name_.end() - 1);
  return true;
}

// static
bool DomainName::Append(const DomainName& first,
                        const DomainName& second,
                        uint8_t result[256]) {
  DCHECK(first.domain_name_.size());
  DCHECK_EQ(first.domain_name_.back(), 0);
  DCHECK(second.domain_name_.size());
  DCHECK_EQ(second.domain_name_.back(), 0);
  if ((first.domain_name_.size() + second.domain_name_.size() - 1) >
      kDomainNameMaxLength) {
    return false;
  }
  auto it = std::copy(first.domain_name_.begin(), first.domain_name_.end() - 1,
                      result);
  std::copy(second.domain_name_.begin(), second.domain_name_.end(), it);
  return true;
}

DomainName::DomainName() : domain_name_{0} {}
DomainName::DomainName(std::vector<uint8_t>&& domain_name)
    : domain_name_(std::move(domain_name)) {
  CHECK_LE(domain_name_.size(), kDomainNameMaxLength);
}
DomainName::DomainName(const DomainName&) = default;
DomainName::DomainName(DomainName&&) = default;
DomainName::~DomainName() = default;
DomainName& DomainName::operator=(const DomainName& domain_name) = default;
DomainName& DomainName::operator=(DomainName&& domain_name) = default;

bool DomainName::operator==(const DomainName& other) const {
  return domain_name_ == other.domain_name_;
}

bool DomainName::operator!=(const DomainName& other) const {
  return !(*this == other);
}

bool DomainName::Append(const DomainName& after) {
  DCHECK(after.domain_name_.size());
  DCHECK_EQ(after.domain_name_.back(), 0);
  if ((domain_name_.size() + after.domain_name_.size() - 1) >
      kDomainNameMaxLength) {
    return false;
  }
  domain_name_.insert(domain_name_.end() - 1, after.domain_name_.begin(),
                      after.domain_name_.end() - 1);
  return true;
}

std::vector<std::string> DomainName::GetLabels() const {
  DCHECK_GT(domain_name_.size(), 0);
  std::vector<std::string> result;
  auto it = domain_name_.begin();
  while (*it != 0) {
    DCHECK_LT(it - domain_name_.begin(), kDomainNameMaxLength);
    DCHECK_LT((it + 1 + *it) - domain_name_.begin(), kDomainNameMaxLength);
    result.emplace_back(it + 1, it + 1 + *it);
    it += 1 + *it;
  }
  return result;
}

std::ostream& operator<<(std::ostream& os, const DomainName& domain_name) {
  const auto& data = domain_name.domain_name();
  DCHECK_GT(data.size(), 0);
  auto it = data.begin();
  while (*it != 0) {
    for (size_t length = *it++; length > 0; ++it, --length) {
      uint8_t c = *it;
      if (c >= ' ' && c <= '~') {
        os.put(c);
      } else {
        // Output a hex escape sequence for non-printable values.
        os.put('\\');
        os.put('x');
        char digit = (c >> 4) & 0xf;
        if (digit >= 0 && digit <= 9) {
          os.put(digit + '0');
        } else {
          os.put(digit - 10 + 'a');
        }
        digit = c & 0xf;
        if (digit >= 0 && digit <= 9) {
          os.put(digit + '0');
        } else {
          os.put(digit - 10 + 'a');
        }
      }
    }
    os << ".";
  }
  return os;
}

}  // namespace mdns
}  // namespace openscreen
