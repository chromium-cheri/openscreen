// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_rdata.h"

#include "absl/strings/match.h"
#include "absl/strings/str_join.h"
#include "platform/api/logging.h"

namespace cast {
namespace mdns {

bool IsValidDomainLabel(const std::string& label) {
  const size_t label_size = label.size();
  return label_size > 0 && label_size <= kMaxLabelLength;
}

void DomainName::Clear() {
  max_wire_size_ = 1;
  labels_.clear();
}

bool DomainName::PushLabel(const std::string& label) {
  if (!IsValidDomainLabel(label)) {
    OSP_DLOG_ERROR << "Invalid domain name label: \"" << label << "\"";
    return false;
  }
  // Include the label length byte in the size calculation.
  // Add terminating character byte to maximum domain length, as it only
  // limits label bytes and label length bytes.
  if (max_wire_size_ + label.size() + 1 > kMaxDomainNameLength + 1) {
    OSP_DLOG_ERROR << "Name too long. Cannot push label: \"" << label << "\"";
    return false;
  }

  labels_.push_back(label);

  // Update the size of the full name in wire format. Include the length byte in
  // the size calculation.
  max_wire_size_ += label.size() + 1;
  return true;
}

const std::string& DomainName::Label(size_t label_index) const {
  OSP_DCHECK(label_index < labels_.size());
  return labels_[label_index];
}

std::string DomainName::ToString() const {
  return absl::StrJoin(labels_, ".");
}

bool DomainName::operator==(const DomainName& rhs) const {
  if (labels_.size() != rhs.labels_.size()) {
    return false;
  }
  for (size_t i = 0; i < labels_.size(); ++i) {
    if (!absl::EqualsIgnoreCase(labels_[i], rhs.labels_[i])) {
      return false;
    }
  }
  return true;
}

bool DomainName::operator!=(const DomainName& rhs) const {
  return !(*this == rhs);
}

std::ostream& operator<<(std::ostream& stream, const DomainName& domain_name) {
  return stream << domain_name.ToString();
}

}  // namespace mdns
}  // namespace cast
