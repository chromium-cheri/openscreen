// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_MDNS_MDNS_RDATA_H_
#define CAST_COMMON_MDNS_MDNS_RDATA_H_

#include <string>
#include <vector>

#include "cast/common/mdns/mdns_constants.h"
#include "osp_base/ip_address.h"

namespace cast {
namespace mdns {

using IPAddress = openscreen::IPAddress;

bool IsValidDomainLabel(const std::string& label);

// Represents domain name as a collection of labels, ensures label length and
// domain name length requirements are met.
class DomainName {
 public:
  DomainName() = default;
  DomainName(const DomainName& other) = default;
  DomainName(DomainName&& other) = default;
  ~DomainName() = default;

  DomainName& operator=(const DomainName& other) = default;
  DomainName& operator=(DomainName&& other) = default;

  bool operator==(const DomainName& rhs) const;
  bool operator!=(const DomainName& rhs) const;

  // Clear removes all previously pushed labels and puts DomainName in its
  // initial state.
  void Clear();
  // Appends the given label to the end of the domain name and returns true if
  // the label is a valid domain label and the domain name does not exceed the
  // maximum length. Returns false otherwise.
  bool PushLabel(const std::string& label);
  // Returns a reference to the label at specified label_index. No bounds
  // checking is performed.
  const std::string& Label(size_t label_index) const;
  std::string ToString() const;

  // Returns the maximum space that the domain name could take up in its
  // on-the-wire format. This is an upper bound based on the length of the
  // labels that make up the domain name. It's possible that with domain name
  // compression the actual space taken in on-the-wire format is smaller.
  size_t max_wire_size() const { return max_wire_size_; }
  bool empty() const { return labels_.empty(); }
  size_t label_count() const { return labels_.size(); }

 private:
  // wire_size_ starts at 1 for the terminating character length.
  size_t max_wire_size_ = 1;
  std::vector<std::string> labels_;
};

std::ostream& operator<<(std::ostream& stream, const DomainName& domain_name);

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_RDATA_H_
