// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_parsing.h"

#include "absl/hash/hash.h"
#include "absl/strings/ascii.h"
#include "cast/common/mdns/mdns_constants.h"
#include "platform/api/logging.h"

namespace cast {
namespace mdns {

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
  const uint8_t* rollback_position = current();
  uint16_t record_length;
  if (Read<uint16_t>(&record_length)) {
    std::vector<uint8_t> buffer(record_length);
    if (ReadBytes(buffer.size(), buffer.data())) {
      out->set_rdata(buffer);
      return true;
    }
  }
  set_current(rollback_position);
  return false;
}

bool MdnsReader::ReadSrvRecordRdata(SrvRecordRdata* out) {
  OSP_DCHECK(out);
  const uint8_t* rollback_position = current();
  uint16_t record_length;
  uint16_t priority;
  uint16_t weight;
  uint16_t port;
  DomainName target;
  if (Read<uint16_t>(&record_length) && Read<uint16_t>(&priority) &&
      Read<uint16_t>(&weight) && Read<uint16_t>(&port) &&
      ReadDomainName(&target) &&
      (current() - rollback_position == record_length)) {
    out->set_priority(priority);
    out->set_weight(weight);
    out->set_port(port);
    out->set_target(target);
    return true;
  }
  set_current(rollback_position);
  return false;
}

bool MdnsReader::ReadARecordRdata(ARecordRdata* out) {
  OSP_DCHECK(out);
  const uint8_t* rollback_position = current();
  uint16_t record_length;
  uint8_t buffer[IPAddress::kV4Size];
  if (Read<uint16_t>(&record_length) && (record_length == sizeof(buffer)) &&
      ReadBytes(sizeof(buffer), buffer)) {
    IPAddress address(buffer);
    OSP_DCHECK(address.IsV4());
    out->set_address(address);
    return true;
  }
  set_current(rollback_position);
  return false;
}

bool MdnsReader::ReadAAAARecordRdata(AAAARecordRdata* out) {
  OSP_DCHECK(out);
  const uint8_t* rollback_position = current();
  uint16_t record_length;
  uint8_t buffer[IPAddress::kV6Size];
  if (Read<uint16_t>(&record_length) && (record_length == sizeof(buffer)) &&
      ReadBytes(sizeof(buffer), buffer)) {
    IPAddress address(buffer);
    OSP_DCHECK(address.IsV6());
    out->set_address(address);
    return true;
  }
  set_current(rollback_position);
  return false;
}

bool MdnsReader::ReadPtrRecordRdata(PtrRecordRdata* out) {
  OSP_DCHECK(out);
  const uint8_t* rollback_position = current();
  uint16_t record_length;
  DomainName ptr_domain;
  if (Read<uint16_t>(&record_length) && ReadDomainName(&ptr_domain) &&
      (current() - rollback_position == record_length)) {
    out->set_ptr_domain(ptr_domain);
    return true;
  }
  set_current(rollback_position);
  return false;
}

// TODO: check if length is indeed as expected

bool MdnsReader::ReadTxtRecordRdata(TxtRecordRdata* out) {
  OSP_DCHECK(out);
  const uint8_t* rollback_position = current();
  uint16_t record_length;
  if (Read<uint16_t>(&record_length)) {
    uint8_t entry_length = 0;
    char buffer[kTXTMaxEntrySize];
    std::vector<std::string> texts;
    // TODO: need to rollback reader in case of failure
    // TODO: must not read more than specified record_length

    while (Read<uint8_t>(&entry_length) && entry_length > 0 &&
           entry_length <= sizeof(buffer) && ReadBytes(entry_length, buffer)) {
      texts.emplace_back(buffer, entry_length);
    }

    // do {
    //   MAKE_SURE(Read<uint8_t>(&entry_length));
    //   MAKE_SURE(entry_length <= sizeof(buffer));
    //   if (!entry_length)
    //     break;  // Found a zero length entry.
    //   MAKE_SURE(ReadBytes(entry_length, buffer));
    //   texts.push_back(std::string(buffer, entry_length));
    // } while (reader->offset() - start_offset < rdlength);

    out->set_texts(std::move(texts));
    return true;
  }
  set_current(rollback_position);
  return false;
}

MdnsWriter::MdnsWriter(uint8_t* buffer, size_t length)
    : BigEndianWriter(buffer, length) {}

namespace {

std::vector<uint64_t> ComputeDomainNameSubhashes(const DomainName& name) {
  // Based on absl Hash128to64 that combines two 64-bit hashes into one
  auto hash_combiner = [](uint64_t seed, const std::string& value) -> uint64_t {
    const uint64_t kMultiplier = UINT64_C(0x9ddfea08eb382d69);
    uint64_t hash_value = absl::Hash<std::string>{}(value);
    uint64_t a = (hash_value ^ seed) * kMultiplier;
    a ^= (a >> 47);
    uint64_t b = (seed ^ a) * kMultiplier;
    b ^= (b >> 47);
    b *= kMultiplier;
    return b;
  };

  // Use a large prime between 2^63 and 2^64 as a starting value.
  // This is taken from absl::Hash implementation.
  uint64_t hash_value = UINT64_C(0xc3a5c85c97cb3127);
  std::vector<uint64_t> subhashes(name.labels_size());
  for (size_t i = name.labels_size(); i-- > 0;) {
    hash_value =
        hash_combiner(hash_value, absl::AsciiStrToLower(name.Label(i)));
    subhashes[i] = hash_value;
  }
  return subhashes;
}

}  // namespace

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

  uint8_t* rollback_position = current();
  std::vector<uint64_t> subhashes = ComputeDomainNameSubhashes(name);
  // Tentative dictionary contains label pointer entries to be added to the
  // compression dictionary after successfully writing the domain name.
  std::unordered_map<uint64_t, uint16_t> tentative_dictionary;
  for (size_t i = 0; i < name.labels_size(); ++i) {
    OSP_DCHECK(IsValidDomainLabel(name.Label(i)));
    // We only need to do a look up in the compression dictionary and not in the
    // tentative dictionary. The tentative dictionary cannot possibly contain a
    // valid label pointer as all the entries previously added to it are for
    // names that are longer than the currently processed sub-name.
    auto find_result = dictionary_.find(subhashes[i]);
    if (find_result != dictionary_.end()) {
      if (!Write<uint16_t>(find_result->second)) {
        set_current(rollback_position);
        return false;
      }
      dictionary_.insert(tentative_dictionary.begin(),
                         tentative_dictionary.end());
      return true;
    }
    // Only add a pointer_label for compression if the offset into the buffer
    // fits into the bits available to store it.
    if (current() - begin() <= kLabelOffsetMask) {
      uint16_t pointer_label = (uint16_t(kLabelPointer) << 8) |
                               ((current() - begin()) & kLabelOffsetMask);
      tentative_dictionary.insert(std::make_pair(subhashes[i], pointer_label));
    }
    const std::string& label = name.Label(i);
    uint8_t direct_label =
        (static_cast<uint8_t>(label.size()) & ~kLabelMask) | kLabelDirect;
    if (!Write<uint8_t>(direct_label) ||
        !WriteBytes(label.data(), label.size())) {
      set_current(rollback_position);
      return false;
    }
  }
  if (!Write<uint8_t>(kLabelTermination)) {
    set_current(rollback_position);
    return false;
  }
  dictionary_.insert(tentative_dictionary.begin(), tentative_dictionary.end());
  return true;
}

bool MdnsWriter::WriteRawRecordRdata(const RawRecordRdata& rdata) {
  if (rdata.rdata().size() > std::numeric_limits<uint16_t>::max()) {
    return false;
  }
  uint8_t* rollback_position = current();
  uint16_t record_length = static_cast<uint16_t>(rdata.rdata().size());
  if (Write<uint16_t>(record_length) &&
      WriteBytes(rdata.rdata().data(), record_length)) {
    return true;
  }
  set_current(rollback_position);
  return false;
}

// TODO: roll back writer on failure in all write methods

bool MdnsWriter::WriteSrvRecordRdata(const SrvRecordRdata& rdata) {
  return Write<uint16_t>(rdata.priority()) && Write<uint16_t>(rdata.weight()) &&
         Write<uint16_t>(rdata.port()) && WriteDomainName(rdata.target());
}

bool MdnsWriter::WriteARecordRdata(const ARecordRdata& rdata) {
  OSP_DCHECK(rdata.address().IsV4());
  uint8_t* rollback_position = current();
  if (Write<uint16_t>(IPAddress::kV4Size)) {
    uint8_t bytes[IPAddress::kV4Size];
    rdata.address().CopyToV4(bytes);
    if (WriteBytes(bytes, sizeof(bytes))) {
      return true;
    }
  }
  set_current(rollback_position);
  return false;
}

bool MdnsWriter::WriteAAAARecordRdata(const AAAARecordRdata& rdata) {
  OSP_DCHECK(rdata.address().IsV6());
  uint8_t* rollback_position = current();
  if (Write<uint16_t>(IPAddress::kV6Size)) {
    uint8_t bytes[IPAddress::kV6Size];
    rdata.address().CopyToV6(bytes);
    if (WriteBytes(bytes, sizeof(bytes))) {
      return true;
    }
  }
  set_current(rollback_position);
  return false;
}

bool MdnsWriter::WritePtrRecordRdata(const PtrRecordRdata& rdata) {
  return WriteDomainName(rdata.ptr_domain());
}

bool MdnsWriter::WriteTxtRecordRdata(const TxtRecordRdata& rdata) {
  // TODO: roll back writer on failure
  // size_t start_offset = writer->offset();
  const std::vector<std::string>& texts = rdata.texts();
  if (!texts.empty()) {
    for (const std::string& entry : texts) {
      // MAKE_SURE(entry.size() <= kTXTMaxEntrySize); DCHECK this? make this
      // part of TxtRecordRdata?
      if (!(Write<uint8_t>(entry.size()) &&
            WriteBytes(entry.data(), entry.size()))) {
        return false;
      }
    }
  } else {
    return Write<uint8_t>(kTXTEmptyRdata);
  }
  //*bytes_written = writer->offset() - start_offset;
  return true;
}

}  // namespace mdns
}  // namespace cast
