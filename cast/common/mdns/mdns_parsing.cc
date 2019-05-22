// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_parsing.h"

#include "absl/hash/hash.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_join.h"
#include "cast/common/mdns/mdns_constants.h"
#include "platform/api/logging.h"

namespace cast {
namespace mdns {

void DomainName::Clear() {
  wire_size_ = 0;
  labels_.clear();
}

bool DomainName::PushLabel(const std::string& label) {
  if (label.empty() || label.size() > kMaxLabelLength) {
    OSP_LOG_ERROR << "Invalid domain name label: \"" << label << "\"";
    return false;
  }
  // Include the length byte in the size calculation.
  if (wire_size_ + label.size() + 1 > kMaxDomainNameLength) {
    OSP_LOG_ERROR << "Name too long. Cannot push label: \"" << label << "\"";
    return false;
  }

  labels_.push_back(label);

  // Update the size of the full name in wire format. Include the length byte in
  // the size calculation.
  wire_size_ += label.size() + 1;
  return true;
}

const std::string& DomainName::Label(size_t label_index) const {
  // TODO: what if index is out of bounds?
  // if (label_index >= labels_.size())
  //  return absl::string_view();
  return labels_[label_index];
}

std::string DomainName::ToString() const {
  return absl::StrJoin(labels_, ".");
}

bool operator==(const DomainName& lhs, const DomainName& rhs) {
  if (lhs.labels_.size() != rhs.labels_.size()) {
    return false;
  }
  for (size_t i = 0; i < lhs.labels_.size(); ++i) {
    if (!absl::EqualsIgnoreCase(lhs.labels_[i], rhs.labels_[i])) {
      return false;
    }
  }
  return true;
}

bool operator!=(const DomainName& lhs, const DomainName& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& stream, const DomainName& domain_name) {
  // TODO: ugh, about this, don't tostring, just output labels to stream
  // directly
  return stream << domain_name.ToString();
}

MdnsReader::MdnsReader(const uint8_t* buffer, size_t length)
    : BigEndianReader(buffer, length) {}

bool MdnsReader::ReadDomainName(DomainName* out) {
  OSP_DCHECK(out);
  out->Clear();

  const uint8_t* position = current();
  size_t total_bytes = length();
  // Number of bytes consumed reading from the starting position to either the
  // first jump or the final termination byte. This should correspond with the
  // number of consecutive bytes the domain name took up.
  size_t consumed_bytes = 0;
  size_t processed_bytes = 0;

  while (true) {
    if (position >= end()) {
      return false;
    }
    if (processed_bytes > total_bytes) {
      // if we have processed more bytes than there are in the buffer
      // we are in a circular compression loop
      return false;
    }
    uint8_t label_type = openscreen::ReadBigEndian<uint8_t>(position);
    if (label_type == kLabelTermination) {
      if (consumed_bytes == 0) {
        consumed_bytes = position + sizeof(uint8_t) - current();
      }
      return Skip(consumed_bytes);
    } else if ((label_type & kLabelMask) == kLabelPointer) {
      if (position + sizeof(uint16_t) > end()) {
        return false;
      }
      uint16_t label_offset =
          openscreen::ReadBigEndian<uint16_t>(position) & kLabelOffsetMask;
      if (consumed_bytes == 0) {
        consumed_bytes = position + sizeof(uint16_t) - current();
      }
      processed_bytes += sizeof(uint16_t);
      position = begin() + label_offset;
    } else if ((label_type & kLabelMask) == kLabelDirect) {
      uint8_t label_length = label_type & ~kLabelMask;
      OSP_DCHECK_NE(label_length, 0);
      position += sizeof(uint8_t);
      processed_bytes += sizeof(uint8_t);
      if (position + label_length >= end()) {
        return false;
      }
      // TODO:
      out->PushLabel(
          std::string(reinterpret_cast<const char*>(position), label_length));
      processed_bytes += label_length;
      position += label_length;
    } else {
      return false;
    }
  }
}

MdnsWriter::MdnsWriter(uint8_t* buffer, size_t length)
    : BigEndianWriter(buffer, length) {}

bool MdnsWriter::WriteDomainName(const DomainName& name,
                                 bool allow_compression) {
  if (name.empty()) {
    return false;
  }

  // similar to boost::hash_combine, since as of now std:: does not have one
  auto hash_combiner = [](size_t seed, const std::string& value) -> size_t {
    return seed ^ (std::hash<std::string>{}(value) + 0x9e3779b9 + (seed << 6) +
                   (seed >> 2));
  };

  size_t previous_hash = 0;
  std::vector<size_t> subhashes(name.labels_size());
  for (size_t i = 0; i < name.labels_size(); ++i) {
    size_t index = name.labels_size() - i - 1;
    size_t current_hash =
        hash_combiner(previous_hash, absl::AsciiStrToLower(name.Label(index)));
    subhashes[index] = current_hash;
    previous_hash = current_hash;
  }
  
  uint8_t* position = current();
  for (size_t i = 0; i < name.labels_size(); ++i) {
    const std::string& label = name.Label(i);

    OSP_DCHECK(!label.empty());
    OSP_DCHECK_LE(label.size(), kMaxLabelLength);
    OSP_DCHECK_EQ(label.size() & kLabelMask, 0);

    if (allow_compression) {
      auto find_result = dictionary_.find(subhashes[i]);
      if (find_result != dictionary_.end()) {
        if (position + sizeof(uint16_t) > end()) {
          return false;
        }
        openscreen::WriteBigEndian<uint16_t>(find_result->second, position);
        position += sizeof(uint16_t);
        return Skip(position - current());
      }
      OSP_DCHECK_EQ((position - begin()) & ~kLabelOffsetMask, 0);
      uint16_t pointer_label = (uint16_t(kLabelPointer) << 8) |
                               ((position - begin()) & kLabelOffsetMask);
      dictionary_.insert(std::make_pair(subhashes[i], pointer_label));
    }
    if (position + sizeof(uint8_t) + label.size() > end()) {
      return false;
    }
    uint8_t direct_label = kLabelDirect | static_cast<uint8_t>(label.size());
    openscreen::WriteBigEndian<uint8_t>(direct_label, position);
    position += sizeof(uint8_t);
    memcpy(position, label.data(), label.size());
    position += label.size();
  }
  if (position + sizeof(uint8_t) > end()) {
    return false;
  }
  openscreen::WriteBigEndian<uint8_t>(kLabelTermination, position);
  position += sizeof(uint8_t);
  Skip(position - current());
  return true;
}

}  // namespace mdns
}  // namespace cast
