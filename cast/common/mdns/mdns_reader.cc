// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_reader.h"

//#include "absl/hash/hash.h"
//#include "absl/strings/ascii.h"
#include "cast/common/mdns/mdns_constants.h"
#include "platform/api/logging.h"

namespace cast {
namespace mdns {

MdnsReader::MdnsReader(const uint8_t* buffer, size_t length)
    : BigEndianReader(buffer, length) {}

bool MdnsReader::ReadCharacterString(std::string* out) {
  const uint8_t* const rollback_position = current();
  uint8_t string_length;
  if (Read<uint8_t>(&string_length)) {
    const char* string_begin = reinterpret_cast<const char*>(current());
    if (Skip(string_length)) {
      *out = std::string(string_begin, string_length);
      return true;
    }
  }
  set_current(rollback_position);
  return false;
}

// RFC 1035: https://www.ietf.org/rfc/rfc1035.txt
// See section 4.1.4. Message compression
bool MdnsReader::ReadDomainName(DomainName* out) {
  OSP_DCHECK(out);
  out->Clear();
  const uint8_t* position = current();
  // The number of bytes consumed reading from the starting position to either
  // the first label pointer or the final termination byte, including the
  // pointer or the termination byte. This is equal to the actual wire size of
  // the DomainName accounting for compression.
  size_t bytes_consumed = 0;
  // The number of bytes that was processed when reading the DomainName,
  // including all label pointers and direct labels. It is used to detect
  // circular compression. The number of processed bytes cannot be possibly
  // greater than the length of the buffer.
  size_t bytes_processed = 0;

  // If we are pointing before the beginning or past the end of the buffer, we
  // hit a malformed pointer. If we have processed more bytes than there are in
  // the buffer, we are in a circular compression loop.
  while (position >= begin() && position < end() &&
         bytes_processed <= length()) {
    const uint8_t label_type = openscreen::ReadBigEndian<uint8_t>(position);
    if (label_type == kLabelTermination) {
      if (bytes_consumed == 0) {
        bytes_consumed = position + sizeof(uint8_t) - current();
      }
      return Skip(bytes_consumed);
    } else if ((label_type & kLabelMask) == kLabelPointer) {
      if (position + sizeof(uint16_t) > end()) {
        return false;
      }
      const uint16_t label_offset =
          openscreen::ReadBigEndian<uint16_t>(position) & kLabelOffsetMask;
      if (bytes_consumed == 0) {
        bytes_consumed = position + sizeof(uint16_t) - current();
      }
      bytes_processed += sizeof(uint16_t);
      position = begin() + label_offset;
    } else if ((label_type & kLabelMask) == kLabelDirect) {
      const uint8_t label_length = label_type & ~kLabelMask;
      OSP_DCHECK_NE(label_length, 0);
      position += sizeof(uint8_t);
      bytes_processed += sizeof(uint8_t);
      if (position + label_length >= end()) {
        return false;
      }
      if (!out->PushLabel(std::string(reinterpret_cast<const char*>(position),
                                      label_length))) {
        return false;
      }
      bytes_processed += label_length;
      position += label_length;
    } else {
      return false;
    }
  }
  return false;
}

bool MdnsReader::ReadRawRecordRdata(RawRecordRdata* out) {
  OSP_DCHECK(out);
  const uint8_t* const rollback_position = current();
  uint16_t record_length;
  if (Read<uint16_t>(&record_length)) {
    std::vector<uint8_t> buffer(record_length);
    if (ReadBytes(buffer.size(), buffer.data())) {
      out->set_rdata(std::move(buffer));
      return true;
    }
  }
  set_current(rollback_position);
  return false;
}

bool MdnsReader::ReadSrvRecordRdata(SrvRecordRdata* out) {
  OSP_DCHECK(out);
  const uint8_t* const rollback_position = current();
  uint16_t record_length;
  uint16_t priority;
  uint16_t weight;
  uint16_t port;
  DomainName target;
  if (Read<uint16_t>(&record_length) && Read<uint16_t>(&priority) &&
      Read<uint16_t>(&weight) && Read<uint16_t>(&port) &&
      ReadDomainName(&target) &&
      (current() - rollback_position ==
       sizeof(record_length) + record_length)) {
    out->set_priority(priority);
    out->set_weight(weight);
    out->set_port(port);
    out->set_target(std::move(target));
    return true;
  }
  set_current(rollback_position);
  return false;
}

bool MdnsReader::ReadARecordRdata(ARecordRdata* out) {
  OSP_DCHECK(out);
  const uint8_t* const rollback_position = current();
  uint16_t record_length;
  if (Read<uint16_t>(&record_length) && (record_length == IPAddress::kV4Size)) {
    const uint8_t* const address_bytes = current();
    if (Skip(IPAddress::kV4Size)) {
      out->set_address(IPAddress(IPAddress::Version::kV4, address_bytes));
      return true;
    }
  }
  set_current(rollback_position);
  return false;
}

bool MdnsReader::ReadAAAARecordRdata(AAAARecordRdata* out) {
  OSP_DCHECK(out);
  const uint8_t* const rollback_position = current();
  uint16_t record_length;
  if (Read<uint16_t>(&record_length) && (record_length == IPAddress::kV6Size)) {
    const uint8_t* const address_bytes = current();
    if (Skip(IPAddress::kV6Size)) {
      out->set_address(IPAddress(IPAddress::Version::kV6, address_bytes));
      return true;
    }
  }
  set_current(rollback_position);
  return false;
}

bool MdnsReader::ReadPtrRecordRdata(PtrRecordRdata* out) {
  OSP_DCHECK(out);
  const uint8_t* const rollback_position = current();
  uint16_t record_length;
  DomainName ptr_domain;
  if (Read<uint16_t>(&record_length) && ReadDomainName(&ptr_domain) &&
      (current() - rollback_position ==
       sizeof(record_length) + record_length)) {
    out->set_ptr_domain(std::move(ptr_domain));
    return true;
  }
  set_current(rollback_position);
  return false;
}

bool MdnsReader::ReadTxtRecordRdata(TxtRecordRdata* out) {
  OSP_DCHECK(out);
  const uint8_t* const rollback_position = current();
  uint16_t record_length;
  if (Read<uint16_t>(&record_length)) {
    std::vector<std::string> texts;
    while (current() - rollback_position <
           sizeof(record_length) + record_length) {
      std::string entry;
      if (!ReadCharacterString(&entry)) {
        set_current(rollback_position);
        return false;
      }
      OSP_DCHECK(entry.length() <= kTXTMaxEntrySize);
      texts.push_back(std::move(entry));
    }
    if (current() - rollback_position ==
        sizeof(record_length) + record_length) {
      out->set_texts(std::move(texts));
      return true;
    }
  }
  set_current(rollback_position);
  return false;
}

}  // namespace mdns
}  // namespace cast
