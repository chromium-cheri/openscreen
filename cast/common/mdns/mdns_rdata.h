// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_MDNS_MDNS_RDATA_H_
#define CAST_COMMON_MDNS_MDNS_RDATA_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "cast/common/mdns/mdns_constants.h"
#include "cast/common/mdns/mdns_parsing.h"
#include "osp_base/ip_address.h"

namespace cast {
namespace mdns {

using IPAddress = openscreen::IPAddress;

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

  uint16_t Type() const;
  size_t MaxWireSize() const;
  bool WriteTo(MdnsWriter* writer, uint16_t* bytes_written) const;
  bool ReadFrom(MdnsReader* reader, uint16_t rdlength);

  void set_rdata(const std::string& rdata) { rdata_ = rdata; }
  const std::string& rdata() const { return rdata_; }

  friend bool operator==(const RawRecordRdata& lhs, const RawRecordRdata& rhs);
  friend bool operator!=(const RawRecordRdata& lhs, const RawRecordRdata& rhs);

 private:
  uint16_t type_ = 0;
  std::string rdata_;
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

  uint16_t Type() const;
  size_t MaxWireSize() const;
  bool WriteTo(MdnsWriter* writer, uint16_t* bytes_written) const;
  bool ReadFrom(MdnsReader* reader, uint16_t rdlength);

  void set_priority(uint16_t priority) { priority_ = priority; }
  uint16_t priority() const { return priority_; }
  void set_weight(uint16_t weight) { weight_ = weight; }
  uint16_t weight() const { return weight_; }
  void set_port(uint16_t port) { port_ = port; }
  uint16_t port() const { return port_; }
  void set_target(const DomainName& target) { target_ = target; }
  const DomainName& target() const { return target_; }

  friend bool operator==(const SrvRecordRdata& lhs, const SrvRecordRdata& rhs);
  friend bool operator!=(const SrvRecordRdata& lhs, const SrvRecordRdata& rhs);

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

  uint16_t Type() const;
  size_t MaxWireSize() const;
  bool WriteTo(MdnsWriter* writer, uint16_t* bytes_written) const;
  bool ReadFrom(MdnsReader* reader, uint16_t rdlength);

  void set_address(const IPAddress& address) { address_ = address; }
  const IPAddress& address() const { return address_; }

  friend bool operator==(const ARecordRdata& lhs, const ARecordRdata& rhs);
  friend bool operator!=(const ARecordRdata& lhs, const ARecordRdata& rhs);

 private:
  IPAddress address_;
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

  uint16_t Type() const;
  size_t MaxWireSize() const;
  bool WriteTo(MdnsWriter* writer, uint16_t* bytes_written) const;
  bool ReadFrom(MdnsReader* reader, uint16_t rdlength);

  void set_address(const IPAddress& address) { address_ = address; }
  const IPAddress& address() const { return address_; }

  friend bool operator==(const AAAARecordRdata& lhs,
                         const AAAARecordRdata& rhs);
  friend bool operator!=(const AAAARecordRdata& lhs,
                         const AAAARecordRdata& rhs);

 private:
  IPAddress address_;
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

  uint16_t Type() const;
  size_t MaxWireSize() const;
  bool WriteTo(MdnsWriter* writer, uint16_t* bytes_written) const;
  bool ReadFrom(MdnsReader* reader, uint16_t rdlength);

  void set_ptr_domain(const DomainName& name) { ptr_domain_ = name; }
  const DomainName& ptr_domain() const { return ptr_domain_; }

  friend bool operator==(const PtrRecordRdata& lhs, const PtrRecordRdata& rhs);
  friend bool operator!=(const PtrRecordRdata& lhs, const PtrRecordRdata& rhs);

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

  // RecordRdata Implementation:
  uint16_t Type() const;
  size_t MaxWireSize() const;
  bool WriteTo(MdnsWriter* writer, uint16_t* bytes_written) const;
  bool ReadFrom(MdnsReader* reader, uint16_t rdlength);

  void set_texts(const std::vector<std::string>& texts) { texts_ = texts; }
  const std::vector<std::string>& texts() const { return texts_; }

  friend bool operator==(const TxtRecordRdata& lhs, const TxtRecordRdata& rhs);
  friend bool operator!=(const TxtRecordRdata& lhs, const TxtRecordRdata& rhs);

 private:
  std::vector<std::string> texts_;
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_RDATA_H_
