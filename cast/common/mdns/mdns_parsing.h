// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_MDNS_MDNS_PARSING_H_
#define CAST_COMMON_MDNS_MDNS_PARSING_H_

#include <stdint.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "absl/strings/string_view.h"
#include "osp_base/big_endian.h"
#include "osp_base/macros.h"

namespace cast {
namespace mdns {

bool IsValidDomainLabel(const std::string& label);

// Represents domain name as a collection of labels, ensures label length and
// domain name length requirements are met.
class DomainName {
 public:
  DomainName();
  DomainName(const DomainName& other) = default;
  DomainName(DomainName&& other) = default;
  ~DomainName() = default;

  DomainName& operator=(const DomainName& other) = default;
  DomainName& operator=(DomainName&& other) = default;

  // Clear removes all previously pushed labels and puts DomainName in its
  // initial state.
  void Clear();
  bool PushLabel(const std::string& label);
  const std::string& Label(size_t label_index) const;
  std::string ToString() const;

  // Returns the maximum space that the domain name will take up in its
  // on-the-wire format. NOTE: We must add a byte for the terminating length.
  size_t max_wire_size() const { return wire_size_ + 1; }
  bool empty() const { return labels_.empty(); }
  size_t labels_size() const { return labels_.size(); }

  friend bool operator==(const DomainName& lhs, const DomainName& rhs);
  friend bool operator!=(const DomainName& lhs, const DomainName& rhs);

 private:
  size_t wire_size_ = 0;
  std::vector<std::string> labels_;
};

std::ostream& operator<<(std::ostream& stream, const DomainName& domain_name);

class MdnsReader : public openscreen::BigEndianReader {
 public:
  MdnsReader(const uint8_t* buffer, size_t length);
  bool ReadDomainName(DomainName* out);
};

class MdnsWriter : public openscreen::BigEndianWriter {
 public:
  MdnsWriter(uint8_t* buffer, size_t length);
  bool WriteDomainName(const DomainName& name);

 private:
  // Domain name compression dictionary
  // Maps hashes of previously written domain (sub)names
  // to the first occurence offsets in the underlying buffer
  std::unordered_map<uint64_t, uint16_t> dictionary_;
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_PARSING_H_
