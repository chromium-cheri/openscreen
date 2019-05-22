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

class DomainName {
 public:
  DomainName() = default;
  DomainName(const DomainName& other) = default;
  DomainName(DomainName&& other) = default;
  ~DomainName() = default;

  DomainName& operator=(const DomainName& other) = default;
  DomainName& operator=(DomainName&& other) = default;

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
  bool WriteDomainName(const DomainName& name, bool allow_compression);

 private:
  // Domain name compression dictionary
  // Maps hashes of previously written domain (sub)names
  // to the first occurence offsets in the underlying buffer
  std::unordered_map<size_t, uint16_t> dictionary_;
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_PARSING_H_
