// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_rdata.h"

#include <memory>

#include "platform/api/logging.h"

// A shortcut macro that will log a message and return false if an operation
// fails, or continue executing if it succeeds.
#define MAKE_SURE(expression)                               \
  do {                                                      \
    if (!(expression)) {                                    \
      OSP_DVLOG << "MAKE_SURE failed: " << #expression; \
      return false;                                         \
    }                                                       \
  } while (0);

namespace cast {
namespace mdns {

RecordRdata::~RecordRdata() = default;

RecordRdata::RecordRdata() = default;

// static
std::unique_ptr<RecordRdata> RecordRdata::CreateFromType(uint16_t type) {
  switch (type) {
    case SrvRecordRdata::kType:
      return std::make_unique<SrvRecordRdata>();
    case ARecordRdata::kType:
      return std::make_unique<ARecordRdata>();
    case AAAARecordRdata::kType:
      return std::make_unique<AAAARecordRdata>();
    case PtrRecordRdata::kType:
      return std::make_unique<PtrRecordRdata>();
    case TxtRecordRdata::kType:
      return std::make_unique<TxtRecordRdata>();
    default:
      OSP_DVLOG << "Defaulting to raw RDATA. No support for type: "
                     << type;
      return std::make_unique<RawRecordRdata>(type);
  }
}

// TODO(maclellant): Unsafe pointer casts should be removed if possible.
// base::Value is an example of using union and move semantics to make a generic
// value.
RawRecordRdata::RawRecordRdata(uint16_t type) : type_(type) {
  // Ensure know RDATA types never generate raw record instance. This would
  // lead to unsafe reinterpret_cast<> calls.
  OSP_DCHECK_NE(type, kTypeSRV);
  OSP_DCHECK_NE(type, kTypeA);
  OSP_DCHECK_NE(type, kTypeAAAA);
  OSP_DCHECK_NE(type, kTypePTR);
  OSP_DCHECK_NE(type, kTypeTXT);
}

RawRecordRdata::~RawRecordRdata() = default;

uint16_t RawRecordRdata::Type() const {
  return type_;
}

bool RawRecordRdata::IsEqual(const RecordRdata* other) const {
  OSP_DCHECK(other);
  if (other->Type() != Type())
    return false;
  const RawRecordRdata* raw_other =
      reinterpret_cast<const RawRecordRdata*>(other);
  return raw_other && rdata_ == raw_other->rdata_;
}

size_t RawRecordRdata::MaxWireSize() const {
  return rdata_.size();
}

bool RawRecordRdata::WriteTo(MdnsWriter* writer,
                             uint16_t* bytes_written) const {
  OSP_DCHECK(writer);
  OSP_DCHECK(bytes_written);
  size_t start_offset = writer->offset();
  MAKE_SURE(writer->WriteBytes(rdata_.data(), rdata_.size()));
  *bytes_written = writer->offset() - start_offset;
  return true;
}

bool RawRecordRdata::ReadFrom(MdnsReader* reader, uint16_t rdlength) {
  OSP_DCHECK(reader);
  size_t start_offset = reader->offset();
  std::vector<char> buffer(rdlength);
  MAKE_SURE(reader->ReadBytes(buffer.size(), &buffer[0]));
  rdata_.assign(&buffer[0], buffer.size());
  MAKE_SURE(reader->offset() - start_offset == rdlength);
  return true;
}

std::unique_ptr<RecordRdata> RawRecordRdata::CreateCopy() const {
  auto record = std::make_unique<RawRecordRdata>(type_);
  record->rdata_ = rdata_;
  return record;
}

SrvRecordRdata::SrvRecordRdata() : priority_(0), weight_(0), port_(0) {}

SrvRecordRdata::~SrvRecordRdata() = default;

uint16_t SrvRecordRdata::Type() const {
  return SrvRecordRdata::kType;
}

bool SrvRecordRdata::IsEqual(const RecordRdata* other) const {
  OSP_DCHECK(other);
  if (other->Type() != Type())
    return false;
  const SrvRecordRdata* srv_other =
      reinterpret_cast<const SrvRecordRdata*>(other);
  return srv_other && weight_ == srv_other->weight_ &&
         port_ == srv_other->port_ && priority_ == srv_other->priority_ &&
         target_ == srv_other->target_;
}

size_t SrvRecordRdata::MaxWireSize() const {
  return target_.max_wire_size() + 3 * sizeof(uint16_t);
}

bool SrvRecordRdata::WriteTo(MdnsWriter* writer,
                             uint16_t* bytes_written) const {
  OSP_DCHECK(writer);
  OSP_DCHECK(bytes_written);
  size_t start_offset = writer->offset();
  MAKE_SURE(writer->Write<uint16_t>(priority_));
  MAKE_SURE(writer->Write<uint16_t>(weight_));
  MAKE_SURE(writer->Write<uint16_t>(port_));
  MAKE_SURE(writer->WriteDomainName(target_));
  *bytes_written = writer->offset() - start_offset;
  return true;
}

bool SrvRecordRdata::ReadFrom(MdnsReader* reader, uint16_t rdlength) {
  OSP_DCHECK(reader);
  size_t start_offset = reader->offset();
  MAKE_SURE(reader->Read<uint16_t>(&priority_));
  MAKE_SURE(reader->Read<uint16_t>(&weight_));
  MAKE_SURE(reader->Read<uint16_t>(&port_));
  MAKE_SURE(reader->ReadDomainName(&target_));
  MAKE_SURE(reader->offset() - start_offset == rdlength);
  return true;
}

std::unique_ptr<RecordRdata> SrvRecordRdata::CreateCopy() const {
  auto record = std::make_unique<SrvRecordRdata>();
  record->priority_ = priority_;
  record->weight_ = weight_;
  record->port_ = port_;
  record->target_ = target_;
  return record;
}

ARecordRdata::ARecordRdata() = default;

ARecordRdata::~ARecordRdata() = default;

uint16_t ARecordRdata::Type() const {
  return ARecordRdata::kType;
}

bool ARecordRdata::IsEqual(const RecordRdata* other) const {
  if (other->Type() != Type())
    return false;
  const ARecordRdata* a_other = reinterpret_cast<const ARecordRdata*>(other);
  return a_other && address_ == a_other->address_;
}

size_t ARecordRdata::MaxWireSize() const {
  return address_.IsV4() ? IPAddress::kV4Size : IPAddress::kV6Size;
}

bool ARecordRdata::WriteTo(MdnsWriter* writer, uint16_t* bytes_written) const {
  OSP_DCHECK(writer);
  OSP_DCHECK(bytes_written);
  MAKE_SURE(address_.IsV4());
  uint8_t bytes[IPAddress::kV4Size] = {};
  address_.CopyToV4(bytes);
  MAKE_SURE(writer->WriteBytes(bytes, sizeof(bytes)));
  *bytes_written = IPAddress::kV4Size;
  return true;
}

bool ARecordRdata::ReadFrom(MdnsReader* reader, uint16_t rdlength) {
  OSP_DCHECK(reader);
  MAKE_SURE(rdlength == IPAddress::kV4Size);
  uint8_t buffer[IPAddress::kV4Size];
  MAKE_SURE(reader->ReadBytes(rdlength, buffer));
  IPAddress new_address(buffer);
  OSP_DCHECK(new_address.IsV4());
  address_ = new_address;
  return true;
}

std::unique_ptr<RecordRdata> ARecordRdata::CreateCopy() const {
  auto record = std::make_unique<ARecordRdata>();
  record->address_ = address_;
  return record;
}

AAAARecordRdata::AAAARecordRdata() = default;

AAAARecordRdata::~AAAARecordRdata() = default;

uint16_t AAAARecordRdata::Type() const {
  return AAAARecordRdata::kType;
}

bool AAAARecordRdata::IsEqual(const RecordRdata* other) const {
  if (other->Type() != Type())
    return false;
  const AAAARecordRdata* aaaa_other =
      reinterpret_cast<const AAAARecordRdata*>(other);
  return aaaa_other && address_ == aaaa_other->address_;
}

size_t AAAARecordRdata::MaxWireSize() const {
  return address_.IsV4() ? IPAddress::kV4Size : IPAddress::kV6Size;
}

bool AAAARecordRdata::WriteTo(MdnsWriter* writer,
                              uint16_t* bytes_written) const {
  OSP_DCHECK(writer);
  OSP_DCHECK(bytes_written);
  MAKE_SURE(address_.IsV6());
  uint8_t bytes[IPAddress::kV6Size] = {};
  address_.CopyToV6(bytes);
  MAKE_SURE(writer->WriteBytes(bytes, sizeof(bytes)));
  *bytes_written = IPAddress::kV6Size;
  return true;
}

bool AAAARecordRdata::ReadFrom(MdnsReader* reader, uint16_t rdlength) {
  OSP_DCHECK(reader);
  MAKE_SURE(rdlength == IPAddress::kV6Size);
  uint8_t buffer[IPAddress::kV6Size];
  MAKE_SURE(reader->ReadBytes(rdlength, buffer));
  IPAddress new_address(buffer);
  OSP_DCHECK(new_address.IsV6());
  address_ = new_address;
  return true;
}

std::unique_ptr<RecordRdata> AAAARecordRdata::CreateCopy() const {
  auto record = std::make_unique<AAAARecordRdata>();
  record->address_ = address_;
  return record;
}

PtrRecordRdata::PtrRecordRdata() = default;

PtrRecordRdata::~PtrRecordRdata() = default;

uint16_t PtrRecordRdata::Type() const {
  return PtrRecordRdata::kType;
}

bool PtrRecordRdata::IsEqual(const RecordRdata* other) const {
  if (other->Type() != Type())
    return false;
  const PtrRecordRdata* ptr_other =
      reinterpret_cast<const PtrRecordRdata*>(other);
  return ptr_other && ptr_domain_ == ptr_other->ptr_domain_;
}

size_t PtrRecordRdata::MaxWireSize() const {
  return ptr_domain_.max_wire_size();
}

bool PtrRecordRdata::WriteTo(MdnsWriter* writer,
                             uint16_t* bytes_written) const {
  OSP_DCHECK(writer);
  OSP_DCHECK(bytes_written);
  size_t start_offset = writer->offset();
  MAKE_SURE(writer->WriteDomainName(ptr_domain_));
  *bytes_written = writer->offset() - start_offset;
  return true;
}

bool PtrRecordRdata::ReadFrom(MdnsReader* reader, uint16_t rdlength) {
  OSP_DCHECK(reader);
  size_t start_offset = reader->offset();
  MAKE_SURE(reader->ReadDomainName(&ptr_domain_));
  MAKE_SURE(reader->offset() - start_offset == rdlength);
  return true;
}

std::unique_ptr<RecordRdata> PtrRecordRdata::CreateCopy() const {
  auto record = std::make_unique<PtrRecordRdata>();
  record->ptr_domain_ = ptr_domain_;
  return record;
}

TxtRecordRdata::TxtRecordRdata() = default;

TxtRecordRdata::~TxtRecordRdata() = default;

uint16_t TxtRecordRdata::Type() const {
  return TxtRecordRdata::kType;
}

bool TxtRecordRdata::IsEqual(const RecordRdata* other) const {
  if (other->Type() != Type())
    return false;
  const TxtRecordRdata* txt_other =
      reinterpret_cast<const TxtRecordRdata*>(other);
  return txt_other && texts_ == txt_other->texts_;
}

size_t TxtRecordRdata::MaxWireSize() const {
  if (texts_.empty())
    return 1;  // We will always return at least 1 NULL byte.
  size_t total_size = 0;
  for (size_t i = 0; i < texts_.size(); ++i)
    total_size += texts_[i].size() + 1;  // Don't forget the length byte.
  return total_size;
}

bool TxtRecordRdata::WriteTo(MdnsWriter* writer,
                             uint16_t* bytes_written) const {
  OSP_DCHECK(writer);
  OSP_DCHECK(bytes_written);
  size_t start_offset = writer->offset();
  if (!texts_.empty()) {
    for (size_t i = 0; i < texts_.size(); ++i) {
      const std::string& entry = texts_[i];
      MAKE_SURE(entry.size() <= kTXTMaxEntrySize);
      MAKE_SURE(writer->Write<uint8_t>(entry.size()));
      MAKE_SURE(writer->WriteBytes(entry.data(), entry.size()));
    }
  } else {
    MAKE_SURE(writer->Write<uint8_t>(kTXTEmptyRdata));
  }
  *bytes_written = writer->offset() - start_offset;
  return true;
}

bool TxtRecordRdata::ReadFrom(MdnsReader* reader, uint16_t rdlength) {
  OSP_DCHECK(reader);
  size_t start_offset = reader->offset();
  uint8_t entry_length = 0;
  char buffer[kTXTMaxEntrySize];

  do {
    MAKE_SURE(reader->Read<uint8_t>(&entry_length));
    MAKE_SURE(entry_length <= sizeof buffer);
    if (!entry_length)
      break;  // Found a zero length entry.
    MAKE_SURE(reader->ReadBytes(entry_length, buffer));
    texts_.push_back(std::string(buffer, entry_length));
  } while (reader->offset() - start_offset < rdlength);

  return true;
}

std::unique_ptr<RecordRdata> TxtRecordRdata::CreateCopy() const {
  auto record = std::make_unique<TxtRecordRdata>();
  record->texts_ = texts_;
  return record;
}

}  // namespace libcast_mdns
}  // namespace openscreen
