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

bool IsValidDomainLabel(const std::string& label) {
  const size_t label_size = label.size();
  return label_size > 0 && label_size <= kMaxLabelLength;
}

void DomainName::Clear() {
  wire_size_ = 0;
  labels_.clear();
}

bool DomainName::PushLabel(const std::string& label) {
  if (!IsValidDomainLabel(label)) {
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

MdnsReader::MdnsReader(const uint8_t* buffer, size_t length)
    : BigEndianReader(buffer, length) {}

// RFC 1035: https://www.ietf.org/rfc/rfc1035.txt
// See section 4.1.4. Message compression
bool MdnsReader::ReadDomainName(DomainName* out) {
  OSP_DCHECK(out);
  out->Clear();

  const uint8_t* position = current();
  size_t bytes_total = length();
  // Number of bytes consumed reading from the starting position to either the
  // first jump or the final termination byte. This should correspond with the
  // number of consecutive bytes the domain name took up.
  size_t bytes_consumed = 0;
  size_t bytes_processed = 0;

  // If we are pointing past the end of the buffer, we hit a malformed pointer.
  // If we have processed more bytes than there are in the buffer, we are in a
  // circular compression loop.
  while (position < end() && bytes_processed <= bytes_total) {
    uint8_t label_type = openscreen::ReadBigEndian<uint8_t>(position);
    if (label_type == kLabelTermination) {
      if (bytes_consumed == 0) {
        bytes_consumed = position + sizeof(uint8_t) - current();
      }
      return Skip(bytes_consumed);
    } else if ((label_type & kLabelMask) == kLabelPointer) {
      if (position + sizeof(uint16_t) > end()) {
        return false;
      }
      uint16_t label_offset =
          openscreen::ReadBigEndian<uint16_t>(position) & kLabelOffsetMask;
      if (bytes_consumed == 0) {
        bytes_consumed = position + sizeof(uint16_t) - current();
      }
      bytes_processed += sizeof(uint16_t);
      position = begin() + label_offset;
    } else if ((label_type & kLabelMask) == kLabelDirect) {
      uint8_t label_length = label_type & ~kLabelMask;
      OSP_DCHECK_NE(label_length, 0);
      position += sizeof(uint8_t);
      bytes_processed += sizeof(uint8_t);
      if (position + label_length >= end()) {
        return false;
      }
      out->PushLabel(
          std::string(reinterpret_cast<const char*>(position), label_length));
      bytes_processed += label_length;
      position += label_length;
    } else {
      return false;
    }
  }
  return false;
}

MdnsWriter::MdnsWriter(uint8_t* buffer, size_t length)
    : BigEndianWriter(buffer, length) {}

// RFC 1035: https://www.ietf.org/rfc/rfc1035.txt
// See section 4.1.4. Message compression
bool MdnsWriter::WriteDomainName(const DomainName& name) {
  if (name.empty()) {
    return false;
  }

#if OSP_DCHECK_IS_ON()
  OSP_DCHECK(!name.empty());
  for (size_t i = 0; i < name.labels_size(); ++i) {
    OSP_DCHECK(IsValidDomainLabel(name.Label(i)))
        << "Invalid domain name label \"" << name.Label(i) << "\" at offset "
        << i;
  }
#endif

  // Similar to boost::hash_combine, since as of now std:: does not have one.
  auto hash_combiner = [](uint64_t seed, const std::string& value) -> uint64_t {
    return seed ^ (std::hash<std::string>{}(value) + 0x9e3779b9 + (seed << 6) +
                   (seed >> 2));
  };

  uint64_t hash_value = 0;
  std::vector<uint64_t> subhashes(name.labels_size());

  for (size_t i = name.labels_size(); i-- > 0;) {
    hash_value =
        hash_combiner(hash_value, absl::AsciiStrToLower(name.Label(i)));
    subhashes[i] = hash_value;
  }

  // Tentative dictionary contains label pointer entries to be added to the
  // compression dictionary after successfully writing the domain name.
  std::unordered_map<uint64_t, uint16_t> tentative_dictionary;
  uint8_t* position = current();
  for (size_t i = 0; i < name.labels_size(); ++i) {
    const std::string& label = name.Label(i);
    OSP_DCHECK(IsValidDomainLabel(label));
    // We only need to do a look up in the compression dictionary and not in the
    // tentative dictionary. The tentative dictionary cannot possibly contain a
    // valid label pointer as all the entries previously added to it are for
    // names that are longer than the currently processed sub-name.
    auto find_result = dictionary_.find(subhashes[i]);
    if (find_result != dictionary_.end()) {
      if (position + sizeof(uint16_t) > end()) {
        return false;
      }
      openscreen::WriteBigEndian<uint16_t>(find_result->second, position);
      position += sizeof(uint16_t);
      if (!Skip(position - current())) {
        return false;
      }
      dictionary_.insert(tentative_dictionary.begin(),
                         tentative_dictionary.end());
      return true;
    }
    OSP_DCHECK_EQ((position - begin()) & ~kLabelOffsetMask, 0);
    uint16_t pointer_label = (uint16_t(kLabelPointer) << 8) |
                             ((position - begin()) & kLabelOffsetMask);
    tentative_dictionary.insert(std::make_pair(subhashes[i], pointer_label));
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
  if (!Skip(position - current())) {
    return false;
  }
  dictionary_.insert(tentative_dictionary.begin(), tentative_dictionary.end());
  return true;
}

}  // namespace mdns
}  // namespace cast
