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

// Parsed represenation of the extra data in a record. Does not include standard
// DNS record data such as TTL, Name, Type and Class.

class RawRecordRdata {
 public:
  // Special case. There is no DNS record for '0', but we use it to distinguish
  // a raw record type that we do not know the identity of.
  static const uint16_t kType = 0;

  explicit RawRecordRdata(uint16_t type);
  RawRecordRdata(const RawRecordRdata& other) = default;
  RawRecordRdata(RawRecordRdata&& other) = default;
  ~RawRecordRdata() = default;

  RawRecordRdata& operator=(const RawRecordRdata& other) = default;
  RawRecordRdata& operator=(RawRecordRdata&& other) = default;

  bool operator==(const RawRecordRdata& rhs) const;
  bool operator!=(const RawRecordRdata& rhs) const;

  size_t max_wire_size() const { return rdata_.size(); }
  uint16_t type() const { return type_; };

  const std::vector<uint8_t>& rdata() const { return rdata_; }
  void set_rdata(const std::vector<uint8_t>& rdata) { rdata_ = rdata; }

 private:
  uint16_t type_ = 0;
  std::vector<uint8_t> rdata_;
};

// SRV record format (http://www.ietf.org/rfc/rfc2782.txt):
// 2 bytes network-order unsigned priority
// 2 bytes network-order unsigned weight
// 2 bytes network-order unsigned port
// target: domain name (on-the-wire representation)
class SrvRecordRdata {
 public:
  SrvRecordRdata() = default;
  SrvRecordRdata(const SrvRecordRdata& other) = default;
  SrvRecordRdata(SrvRecordRdata&& other) = default;
  ~SrvRecordRdata() = default;

  SrvRecordRdata& operator=(const SrvRecordRdata& other) = default;
  SrvRecordRdata& operator=(SrvRecordRdata&& other) = default;

  bool operator==(const SrvRecordRdata& rhs) const;
  bool operator!=(const SrvRecordRdata& rhs) const;

  size_t max_wire_size() const;

  uint16_t priority() const { return priority_; }
  void set_priority(uint16_t priority) { priority_ = priority; }
  uint16_t weight() const { return weight_; }
  void set_weight(uint16_t weight) { weight_ = weight; }
  uint16_t port() const { return port_; }
  void set_port(uint16_t port) { port_ = port; }
  const DomainName& target() const { return target_; }
  void set_target(const DomainName& target) { target_ = target; }

 private:
  uint16_t priority_ = 0;
  uint16_t weight_ = 0;
  uint16_t port_ = 0;
  DomainName target_;
};

// A Record format (http://www.ietf.org/rfc/rfc1035.txt):
// 4 bytes for IP address.
class ARecordRdata {
 public:
  ARecordRdata() = default;
  ARecordRdata(const ARecordRdata& other) = default;
  ARecordRdata(ARecordRdata&& other) = default;
  ~ARecordRdata() = default;

  ARecordRdata& operator=(const ARecordRdata& other) = default;
  ARecordRdata& operator=(ARecordRdata&& other) = default;

  bool operator==(const ARecordRdata& rhs) const;
  bool operator!=(const ARecordRdata& rhs) const;

  size_t max_wire_size() const { return openscreen::IPAddress::kV4Size; };

  const openscreen::IPAddress& address() const { return address_; }
  void set_address(const openscreen::IPAddress& address) { address_ = address; }

 private:
  openscreen::IPAddress address_;
};

// AAAA Record format (http://www.ietf.org/rfc/rfc1035.txt):
// 16 bytes for IP address.
class AAAARecordRdata {
 public:
  AAAARecordRdata() = default;
  AAAARecordRdata(const AAAARecordRdata& other) = default;
  AAAARecordRdata(AAAARecordRdata&& other) = default;
  ~AAAARecordRdata() = default;

  AAAARecordRdata& operator=(const AAAARecordRdata& other) = default;
  AAAARecordRdata& operator=(AAAARecordRdata&& other) = default;

  bool operator==(const AAAARecordRdata& rhs) const;
  bool operator!=(const AAAARecordRdata& rhs) const;

  size_t max_wire_size() const { return openscreen::IPAddress::kV6Size; }

  const openscreen::IPAddress& address() const { return address_; }
  void set_address(const openscreen::IPAddress& address) { address_ = address; }

 private:
  openscreen::IPAddress address_;
};

// PTR record format (http://www.ietf.org/rfc/rfc1035.txt):
// domain: On the wire representation of domain name.
class PtrRecordRdata {
 public:
  PtrRecordRdata() = default;
  PtrRecordRdata(const PtrRecordRdata& other) = default;
  PtrRecordRdata(PtrRecordRdata&& other) = default;
  ~PtrRecordRdata() = default;

  PtrRecordRdata& operator=(const PtrRecordRdata& other) = default;
  PtrRecordRdata& operator=(PtrRecordRdata&& other) = default;

  bool operator==(const PtrRecordRdata& rhs) const;
  bool operator!=(const PtrRecordRdata& rhs) const;

  size_t max_wire_size() const { return ptr_domain_.max_wire_size(); }

  const DomainName& ptr_domain() const { return ptr_domain_; }
  void set_ptr_domain(const DomainName& name) { ptr_domain_ = name; }

 private:
  DomainName ptr_domain_;
};

// TXT record format (http://www.ietf.org/rfc/rfc1035.txt):
// texts: One or more <character-string>s.
// a <character-string> is a length octet followed by as many characters.
class TxtRecordRdata {
 public:
  TxtRecordRdata() = default;
  TxtRecordRdata(const TxtRecordRdata& other) = default;
  TxtRecordRdata(TxtRecordRdata&& other) = default;
  ~TxtRecordRdata() = default;

  TxtRecordRdata& operator=(const TxtRecordRdata& other) = default;
  TxtRecordRdata& operator=(TxtRecordRdata&& other) = default;

  bool operator==(const TxtRecordRdata& rhs) const;
  bool operator!=(const TxtRecordRdata& rhs) const;

  size_t max_wire_size() const;

  const std::vector<std::string>& texts() const { return texts_; }
  void set_texts(const std::vector<std::string>& texts) { texts_ = texts; }

 private:
  std::vector<std::string> texts_;
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_RDATA_H_
