// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_MDNS_MDNS_RDATA_H_
#define CAST_COMMON_MDNS_MDNS_RDATA_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "osp_base/ip_address.h"
#include "cast/common/mdns/mdns_constants.h"
#include "cast/common/mdns/mdns_parsing.h"

namespace cast {
namespace mdns {

using IPAddress = openscreen::IPAddress;

// Parsed represenation of the extra data in a record. Does not include standard
// DNS record data such as TTL, Name, Type and Class.
class RecordRdata {
 public:
  static std::unique_ptr<RecordRdata> CreateFromType(uint16_t type);

  virtual ~RecordRdata();

  virtual bool IsEqual(const RecordRdata* other) const = 0;
  virtual uint16_t Type() const = 0;

  virtual size_t MaxWireSize() const = 0;
  virtual bool WriteTo(MdnsWriter* writer, uint16_t* bytes_written) const = 0;
  virtual bool ReadFrom(MdnsReader* reader, uint16_t rdlength) = 0;
  virtual std::unique_ptr<RecordRdata> CreateCopy() const = 0;

 protected:
  RecordRdata();

 private:
  OSP_DISALLOW_COPY_AND_ASSIGN(RecordRdata);
};

class RawRecordRdata : public RecordRdata {
 public:
  // Special case. There is no DNS record for '0', but we use it to distinguish
  // a raw record type that we do not know the identity of.
  static const uint16_t kType = 0;

  explicit RawRecordRdata(uint16_t type);
  ~RawRecordRdata() override;

  // RecordRdata Implementation:
  bool IsEqual(const RecordRdata* other) const override;
  uint16_t Type() const override;
  size_t MaxWireSize() const override;
  bool WriteTo(MdnsWriter* writer, uint16_t* bytes_written) const override;
  bool ReadFrom(MdnsReader* reader, uint16_t rdlength) override;
  std::unique_ptr<RecordRdata> CreateCopy() const override;

  void set_rdata(const std::string& rdata) { rdata_ = rdata; }
  const std::string& rdata() const { return rdata_; }

 private:
  const uint16_t type_;
  std::string rdata_;

  OSP_DISALLOW_COPY_AND_ASSIGN(RawRecordRdata);
};

// SRV record format (http://www.ietf.org/rfc/rfc2782.txt):
// 2 bytes network-order unsigned priority
// 2 bytes network-order unsigned weight
// 2 bytes network-order unsigned port
// target: domain name (on-the-wire representation)
class SrvRecordRdata : public RecordRdata {
 public:
  static const uint16_t kType = kTypeSRV;

  SrvRecordRdata();
  ~SrvRecordRdata() override;

  // RecordRdata Implementation:
  bool IsEqual(const RecordRdata* other) const override;
  uint16_t Type() const override;
  size_t MaxWireSize() const override;
  bool WriteTo(MdnsWriter* writer, uint16_t* bytes_written) const override;
  bool ReadFrom(MdnsReader* reader, uint16_t rdlength) override;
  std::unique_ptr<RecordRdata> CreateCopy() const override;

  void set_priority(uint16_t priority) { priority_ = priority; }
  uint16_t priority() const { return priority_; }
  void set_weight(uint16_t weight) { weight_ = weight; }
  uint16_t weight() const { return weight_; }
  void set_port(uint16_t port) { port_ = port; }
  uint16_t port() const { return port_; }
  void set_target(const DomainName& target) { target_ = target; }
  const DomainName& target() const { return target_; }

 private:
  uint16_t priority_;
  uint16_t weight_;
  uint16_t port_;
  DomainName target_;

  OSP_DISALLOW_COPY_AND_ASSIGN(SrvRecordRdata);
};

// A Record format (http://www.ietf.org/rfc/rfc1035.txt):
// 4 bytes for IP address.
class ARecordRdata : public RecordRdata {
 public:
  static const uint16_t kType = kTypeA;

  ARecordRdata();
  ~ARecordRdata() override;

  // RecordRdata Implementation:
  bool IsEqual(const RecordRdata* other) const override;
  uint16_t Type() const override;
  size_t MaxWireSize() const override;
  bool WriteTo(MdnsWriter* writer, uint16_t* bytes_written) const override;
  bool ReadFrom(MdnsReader* reader, uint16_t rdlength) override;
  std::unique_ptr<RecordRdata> CreateCopy() const override;

  void set_address(const IPAddress& address) { address_ = address; }
  const IPAddress& address() const { return address_; }

 private:
  IPAddress address_;

  OSP_DISALLOW_COPY_AND_ASSIGN(ARecordRdata);
};

// AAAA Record format (http://www.ietf.org/rfc/rfc1035.txt):
// 16 bytes for IP address.
class AAAARecordRdata : public RecordRdata {
 public:
  static const uint16_t kType = kTypeAAAA;

  AAAARecordRdata();
  ~AAAARecordRdata() override;

  // RecordRdata Implementation:
  bool IsEqual(const RecordRdata* other) const override;
  uint16_t Type() const override;
  size_t MaxWireSize() const override;
  bool WriteTo(MdnsWriter* writer, uint16_t* bytes_written) const override;
  bool ReadFrom(MdnsReader* reader, uint16_t rdlength) override;
  std::unique_ptr<RecordRdata> CreateCopy() const override;

  void set_address(const IPAddress& address) { address_ = address; }
  const IPAddress& address() const { return address_; }

 private:
  IPAddress address_;

  OSP_DISALLOW_COPY_AND_ASSIGN(AAAARecordRdata);
};

// PTR record format (http://www.ietf.org/rfc/rfc1035.txt):
// domain: On the wire representation of domain name.
class PtrRecordRdata : public RecordRdata {
 public:
  static const uint16_t kType = kTypePTR;

  PtrRecordRdata();
  ~PtrRecordRdata() override;

  // RecordRdata Implementation:
  bool IsEqual(const RecordRdata* other) const override;
  uint16_t Type() const override;
  size_t MaxWireSize() const override;
  bool WriteTo(MdnsWriter* writer, uint16_t* bytes_written) const override;
  bool ReadFrom(MdnsReader* reader, uint16_t rdlength) override;
  std::unique_ptr<RecordRdata> CreateCopy() const override;

  void set_ptr_domain(const DomainName& name) { ptr_domain_ = name; }
  const DomainName& ptr_domain() const { return ptr_domain_; }

 private:
  DomainName ptr_domain_;

  OSP_DISALLOW_COPY_AND_ASSIGN(PtrRecordRdata);
};

// TXT record format (http://www.ietf.org/rfc/rfc1035.txt):
// texts: One or more <character-string>s.
// a <character-string> is a length octet followed by as many characters.
class TxtRecordRdata : public RecordRdata {
 public:
  static const uint16_t kType = kTypeTXT;

  TxtRecordRdata();
  ~TxtRecordRdata() override;

  // RecordRdata Implementation:
  bool IsEqual(const RecordRdata* other) const override;
  uint16_t Type() const override;
  size_t MaxWireSize() const override;
  bool WriteTo(MdnsWriter* writer, uint16_t* bytes_written) const override;
  bool ReadFrom(MdnsReader* reader, uint16_t rdlength) override;
  std::unique_ptr<RecordRdata> CreateCopy() const override;

  void set_texts(const std::vector<std::string>& texts) { texts_ = texts; }
  const std::vector<std::string>& texts() const { return texts_; }

 private:
  std::vector<std::string> texts_;

  OSP_DISALLOW_COPY_AND_ASSIGN(TxtRecordRdata);
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_RDATA_H_
