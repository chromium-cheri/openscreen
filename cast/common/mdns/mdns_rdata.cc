// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_rdata.h"

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_join.h"
#include "platform/api/logging.h"

namespace cast {
namespace mdns {

bool IsValidDomainLabel(const std::string& label) {
  const size_t label_size = label.size();
  return label_size > 0 && label_size <= kMaxLabelLength;
}

DomainName::DomainName() = default;
DomainName::DomainName(const DomainName&) = default;
DomainName::DomainName(DomainName&&) = default;
DomainName::~DomainName() = default;

DomainName& DomainName::operator=(const DomainName&) = default;
DomainName& DomainName::operator=(DomainName&&) = default;

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

// static
// std::unique_ptr<RecordRdata> RecordRdata::CreateFromType(uint16_t type) {
//   switch (type) {
//     case SrvRecordRdata::kType:
//       return std::make_unique<SrvRecordRdata>();
//     case ARecordRdata::kType:
//       return std::make_unique<ARecordRdata>();
//     case AAAARecordRdata::kType:
//       return std::make_unique<AAAARecordRdata>();
//     case PtrRecordRdata::kType:
//       return std::make_unique<PtrRecordRdata>();
//     case TxtRecordRdata::kType:
//       return std::make_unique<TxtRecordRdata>();
//     default:
//       OSP_DVLOG << "Defaulting to raw RDATA. No support for type: "
//                      << type;
//       return std::make_unique<RawRecordRdata>(type);
//   }
// }

// TODO(maclellant): Unsafe pointer casts should be removed if possible.
// base::Value is an example of using union and move semantics to make a generic
// value.
RawRecordRdata::RawRecordRdata(uint16_t type) : type_(type) {
  // Ensure known RDATA types never generate raw record instance. This would
  // lead to unsafe reinterpret_cast<> calls.
  OSP_DCHECK_NE(type, kTypeSRV);
  OSP_DCHECK_NE(type, kTypeA);
  OSP_DCHECK_NE(type, kTypeAAAA);
  OSP_DCHECK_NE(type, kTypePTR);
  OSP_DCHECK_NE(type, kTypeTXT);
}

RawRecordRdata::RawRecordRdata(const RawRecordRdata&) = default;
RawRecordRdata::RawRecordRdata(RawRecordRdata&&) = default;
RawRecordRdata::~RawRecordRdata() = default;

RawRecordRdata& RawRecordRdata::operator=(const RawRecordRdata&) = default;
RawRecordRdata& RawRecordRdata::operator=(RawRecordRdata&&) = default;

bool RawRecordRdata::operator==(const RawRecordRdata& rhs) const {
  return (type_ == rhs.type_) && (rdata_ == rhs.rdata_);
}

bool RawRecordRdata::operator!=(const RawRecordRdata& rhs) const {
  return !(*this == rhs);
}

SrvRecordRdata::SrvRecordRdata() = default;
SrvRecordRdata::SrvRecordRdata(const SrvRecordRdata&) = default;
SrvRecordRdata::SrvRecordRdata(SrvRecordRdata&&) = default;
SrvRecordRdata::~SrvRecordRdata() = default;

SrvRecordRdata& SrvRecordRdata::operator=(const SrvRecordRdata&) = default;
SrvRecordRdata& SrvRecordRdata::operator=(SrvRecordRdata&&) = default;

bool SrvRecordRdata::operator==(const SrvRecordRdata& rhs) const {
  return (priority_ == rhs.priority_) && (weight_ == rhs.weight_) &&
         (port_ == rhs.port_) && (target_ == rhs.target_);
}

bool SrvRecordRdata::operator!=(const SrvRecordRdata& rhs) const {
  return !(*this == rhs);
}

size_t SrvRecordRdata::max_wire_size() const {
  return sizeof(priority_) + sizeof(weight_) + sizeof(port_) +
         target_.max_wire_size();
}

ARecordRdata::ARecordRdata() = default;
ARecordRdata::ARecordRdata(const ARecordRdata&) = default;
ARecordRdata::ARecordRdata(ARecordRdata&&) = default;
ARecordRdata::~ARecordRdata() = default;

ARecordRdata& ARecordRdata::operator=(const ARecordRdata&) = default;
ARecordRdata& ARecordRdata::operator=(ARecordRdata&&) = default;

bool ARecordRdata::operator==(const ARecordRdata& rhs) const {
  return address_ == rhs.address_;
}

bool ARecordRdata::operator!=(const ARecordRdata& rhs) const {
  return !(*this == rhs);
}

AAAARecordRdata::AAAARecordRdata() = default;
AAAARecordRdata::AAAARecordRdata(const AAAARecordRdata&) = default;
AAAARecordRdata::AAAARecordRdata(AAAARecordRdata&&) = default;
AAAARecordRdata::~AAAARecordRdata() = default;

AAAARecordRdata& AAAARecordRdata::operator=(const AAAARecordRdata&) = default;
AAAARecordRdata& AAAARecordRdata::operator=(AAAARecordRdata&&) = default;

bool AAAARecordRdata::operator==(const AAAARecordRdata& rhs) const {
  return address_ == rhs.address_;
}

bool AAAARecordRdata::operator!=(const AAAARecordRdata& rhs) const {
  return !(*this == rhs);
}

PtrRecordRdata::PtrRecordRdata() = default;
PtrRecordRdata::PtrRecordRdata(const PtrRecordRdata&) = default;
PtrRecordRdata::PtrRecordRdata(PtrRecordRdata&&) = default;
PtrRecordRdata::~PtrRecordRdata() = default;

PtrRecordRdata& PtrRecordRdata::operator=(const PtrRecordRdata&) = default;
PtrRecordRdata& PtrRecordRdata::operator=(PtrRecordRdata&&) = default;

bool PtrRecordRdata::operator==(const PtrRecordRdata& rhs) const {
  return ptr_domain_ == rhs.ptr_domain_;
}

bool PtrRecordRdata::operator!=(const PtrRecordRdata& rhs) const {
  return !(*this == rhs);
}

TxtRecordRdata::TxtRecordRdata() = default;
TxtRecordRdata::TxtRecordRdata(const TxtRecordRdata&) = default;
TxtRecordRdata::TxtRecordRdata(TxtRecordRdata&&) = default;
TxtRecordRdata::~TxtRecordRdata() = default;

TxtRecordRdata& TxtRecordRdata::operator=(const TxtRecordRdata&) = default;
TxtRecordRdata& TxtRecordRdata::operator=(TxtRecordRdata&&) = default;

bool TxtRecordRdata::operator==(const TxtRecordRdata& rhs) const {
  return texts_ == rhs.texts_;
}

bool TxtRecordRdata::operator!=(const TxtRecordRdata& rhs) const {
  return !(*this == rhs);
}

size_t TxtRecordRdata::max_wire_size() const {
  if (texts_.empty()) {
    return 1;  // We will always return at least 1 NULL byte.
  }
  size_t total_size = 0;
  for (const auto& entry : texts_) {
    total_size += entry.size() + 1;  // Don't forget the length byte.
  }
  return total_size;
}

}  // namespace mdns
}  // namespace cast
