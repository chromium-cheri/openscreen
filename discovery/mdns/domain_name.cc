// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/domain_name.h"

#include <algorithm>

#include "platform/api/logging.h"

namespace openscreen {
namespace mdns {

constexpr uint8_t kDomainNameMaxLabelLength = 63u;
constexpr uint16_t kDomainNameMaxLength = 256u;

// static
DomainName DomainName::Append(const DomainName& first,
                              const DomainName& second) {
  DCHECK(first.domain_name.size());
  DCHECK_LE(first.domain_name.size(), kDomainNameMaxLength);
  DCHECK_EQ(first.domain_name.back(), 0);
  DCHECK(second.domain_name.size());
  DCHECK_LE(second.domain_name.size(), kDomainNameMaxLength);
  DCHECK_EQ(second.domain_name.back(), 0);
  DCHECK_LE(first.domain_name.size() + second.domain_name.size() - 1,
            kDomainNameMaxLength);
  DomainName result{{}};
  result.domain_name.reserve(first.domain_name.size() +
                             second.domain_name.size() - 1);
  result.domain_name.resize(first.domain_name.size());
  std::copy(first.domain_name.begin(), first.domain_name.end(),
            result.domain_name.begin());
  result.Append(second);
  return result;
}

// static
void DomainName::Append(const DomainName& first,
                        const DomainName& second,
                        uint8_t* result) {
  DCHECK(first.domain_name.size());
  DCHECK_LE(first.domain_name.size(), kDomainNameMaxLength);
  DCHECK_EQ(first.domain_name.back(), 0);
  DCHECK(second.domain_name.size());
  DCHECK_LE(second.domain_name.size(), kDomainNameMaxLength);
  DCHECK_EQ(second.domain_name.back(), 0);
  DCHECK_LE(first.domain_name.size() + second.domain_name.size() - 1,
            kDomainNameMaxLength);
  DCHECK(result);
  auto it =
      std::copy(first.domain_name.begin(), first.domain_name.end() - 1, result);
  std::copy(second.domain_name.begin(), second.domain_name.end(), it);
}

// static
bool DomainName::FromLabels(const std::vector<std::string>& labels,
                            DomainName* result) {
  DCHECK(result);
  int total_length = 1;
  for (const auto& label : labels) {
    if (label.size() > kDomainNameMaxLabelLength) {
      return false;
    }
    total_length += label.size() + 1;
  }
  if (total_length > kDomainNameMaxLength) {
    return false;
  }
  result->domain_name.resize(total_length);
  auto result_it = result->domain_name.begin();
  for (const auto& label : labels) {
    *result_it++ = static_cast<uint8_t>(label.size());
    result_it = std::copy(label.begin(), label.end(), result_it);
  }
  *result_it = 0;
  return true;
}

DomainName::DomainName() : domain_name{0} {}
DomainName::DomainName(std::vector<uint8_t> domain_name)
    : domain_name(std::move(domain_name)) {}
DomainName::DomainName(const DomainName&) = default;
DomainName::DomainName(DomainName&&) = default;
DomainName::~DomainName() = default;
DomainName& DomainName::operator=(const DomainName&) = default;
DomainName& DomainName::operator=(DomainName&&) = default;

bool DomainName::operator==(const DomainName& other) const {
  return domain_name == other.domain_name;
}

bool DomainName::operator!=(const DomainName& other) const {
  return !(*this == other);
}

DomainName& DomainName::Append(const DomainName& after) {
  DCHECK(after.domain_name.size());
  DCHECK_LE(after.domain_name.size(), kDomainNameMaxLength);
  DCHECK_EQ(after.domain_name.back(), 0);
  DCHECK_LE(domain_name.size() + after.domain_name.size() - 1,
            kDomainNameMaxLength);
  const auto original_size = domain_name.size();
  domain_name.resize(original_size + after.domain_name.size() - 1);
  std::copy(after.domain_name.begin(), after.domain_name.end(),
            domain_name.begin() + original_size - 1);
  return *this;
}

std::vector<std::string> DomainName::GetLabels() const {
  DCHECK_GT(domain_name.size(), 0);
  std::vector<std::string> result;
  auto it = domain_name.begin();
  while (*it != 0) {
    DCHECK_LT(it - domain_name.begin(), kDomainNameMaxLength);
    DCHECK_LT((it + 1 + *it) - domain_name.begin(), kDomainNameMaxLength);
    result.emplace_back(it + 1, it + 1 + *it);
    it += 1 + *it;
  }
  return result;
}

std::ostream& operator<<(std::ostream& os, const DomainName& domain_name) {
  DCHECK_GT(domain_name.domain_name.size(), 0);
  auto it = domain_name.domain_name.begin();
  while (*it != 0) {
    DCHECK_LT(it - domain_name.domain_name.begin(), kDomainNameMaxLength);
    DCHECK_LT((it + 1 + *it) - domain_name.domain_name.begin(),
              kDomainNameMaxLength);
    os.write(reinterpret_cast<const char*>(&*(it + 1)), *it) << ".";
    it += 1 + *it;
  }
  return os;
}

}  // namespace mdns
}  // namespace openscreen
