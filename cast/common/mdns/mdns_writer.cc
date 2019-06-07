// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_writer.h"

#include "absl/hash/hash.h"
#include "absl/strings/ascii.h"
#include "cast/common/mdns/mdns_constants.h"
#include "platform/api/logging.h"

namespace cast {
namespace mdns {

MdnsWriter::MdnsWriter(uint8_t* buffer, size_t length)
    : BigEndianWriter(buffer, length) {}

namespace {

std::vector<uint64_t> ComputeDomainNameSubhashes(const DomainName& name) {
  // Based on absl Hash128to64 that combines two 64-bit hashes into one
  auto hash_combiner = [](uint64_t seed, const std::string& value) -> uint64_t {
    static const uint64_t kMultiplier = UINT64_C(0x9ddfea08eb382d69);
    const uint64_t hash_value = absl::Hash<std::string>{}(value);
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
  std::vector<uint64_t> subhashes(name.label_count());
  for (size_t i = name.label_count(); i-- > 0;) {
    hash_value =
        hash_combiner(hash_value, absl::AsciiStrToLower(name.Label(i)));
    subhashes[i] = hash_value;
  }
  return subhashes;
}

// This helper method writes the number of bytes between |begin| and |end| minus
// the size of the uint16_t into the uint16_t length field at |begin|. The
// method returns true if the number of bytes between |begin| and |end| fits in
// uint16_t type, returns false otherwise.
bool UpdateRecordLength(const uint8_t* end, uint8_t* begin) {
  OSP_DCHECK_LE(begin + sizeof(uint16_t), end);
  ptrdiff_t record_length = end - begin - sizeof(uint16_t);
  if (record_length <= std::numeric_limits<uint16_t>::max()) {
    openscreen::WriteBigEndian<uint16_t>(record_length, begin);
    return true;
  }
  return false;
}

}  // namespace

bool MdnsWriter::WriteCharacterString(const absl::string_view& value) {
  if (value.length() > std::numeric_limits<uint8_t>::max()) {
    return false;
  }
  uint8_t* const rollback_position = current();
  if (Write<uint8_t>(static_cast<uint8_t>(value.length())) &&
      WriteBytes(value.data(), value.length())) {
    return true;
  }
  set_current(rollback_position);
  return false;
}

// RFC 1035: https://www.ietf.org/rfc/rfc1035.txt
// See section 4.1.4. Message compression
bool MdnsWriter::WriteDomainName(const DomainName& name) {
  if (name.empty()) {
    return false;
  }

  uint8_t* const rollback_position = current();
  const std::vector<uint64_t> subhashes = ComputeDomainNameSubhashes(name);
  // Tentative dictionary contains label pointer entries to be added to the
  // compression dictionary after successfully writing the domain name.
  std::unordered_map<uint64_t, uint16_t> tentative_dictionary;
  for (size_t i = 0; i < name.label_count(); ++i) {
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
      const uint16_t pointer_label = (uint16_t(kLabelPointer) << 8) |
                                     ((current() - begin()) & kLabelOffsetMask);
      tentative_dictionary.insert(std::make_pair(subhashes[i], pointer_label));
    }
    const std::string& label = name.Label(i);
    const uint8_t direct_label =
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
  // The probability of a collision is extremely low in this application, as the
  // number of domain names compressed is insignificant in comparison to the
  // hash function image.
  dictionary_.insert(tentative_dictionary.begin(), tentative_dictionary.end());
  return true;
}

bool MdnsWriter::WriteRawRecordRdata(const RawRecordRdata& rdata) {
  if (rdata.rdata().size() > std::numeric_limits<uint16_t>::max()) {
    return false;
  }
  uint8_t* const rollback_position = current();
  if (Write<uint16_t>(static_cast<uint16_t>(rdata.rdata().size())) &&
      WriteBytes(rdata.rdata().data(), rdata.rdata().size())) {
    return true;
  }
  set_current(rollback_position);
  return false;
}

bool MdnsWriter::WriteSrvRecordRdata(const SrvRecordRdata& rdata) {
  uint8_t* const rollback_position = current();
  // Leave space at the beginning at |rollback_position| to write the record
  // length. Cannot write it upfront, since the exact space taken by the target
  // domain name is not known as it might be compressed.
  if (Skip(sizeof(uint16_t)) && Write<uint16_t>(rdata.priority()) &&
      Write<uint16_t>(rdata.weight()) && Write<uint16_t>(rdata.port()) &&
      WriteDomainName(rdata.target()) &&
      UpdateRecordLength(current(), rollback_position)) {
    return true;
  }
  set_current(rollback_position);
  return false;
}

bool MdnsWriter::WriteARecordRdata(const ARecordRdata& rdata) {
  uint8_t* const rollback_position = current();
  uint8_t bytes[IPAddress::kV4Size];
  rdata.ipv4_address().CopyToV4(bytes);
  if (Write<uint16_t>(IPAddress::kV4Size) && WriteBytes(bytes, sizeof(bytes))) {
    return true;
  }
  set_current(rollback_position);
  return false;
}

bool MdnsWriter::WriteAAAARecordRdata(const AAAARecordRdata& rdata) {
  uint8_t* const rollback_position = current();
  uint8_t bytes[IPAddress::kV6Size];
  rdata.ipv6_address().CopyToV6(bytes);
  if (Write<uint16_t>(IPAddress::kV6Size) && WriteBytes(bytes, sizeof(bytes))) {
    return true;
  }
  set_current(rollback_position);
  return false;
}

bool MdnsWriter::WritePtrRecordRdata(const PtrRecordRdata& rdata) {
  uint8_t* const rollback_position = current();
  // Leave space at the beginning at |rollback_position| to write the record
  // length. Cannot write it upfront, since the exact space taken by the target
  // domain name is not known as it might be compressed.
  if (Skip(sizeof(uint16_t)) && WriteDomainName(rdata.ptr_domain()) &&
      UpdateRecordLength(current(), rollback_position)) {
    return true;
  }
  set_current(rollback_position);
  return false;
}

bool MdnsWriter::WriteTxtRecordRdata(const TxtRecordRdata& rdata) {
  uint8_t* const rollback_position = current();

  // size_t start_offset = writer->offset();
  const std::vector<std::string>& texts = rdata.texts();
  if (!texts.empty()) {
    // Leave space at the beginning at |rollback_position| to write the record
    // length. It's cheaper to update it at the end than precompute the length.
    if (Skip(sizeof(uint16_t))) {
      for (const std::string& entry : texts) {
        OSP_DCHECK(entry.length() <= kTXTMaxEntrySize);
        if (!WriteCharacterString(entry)) {
          set_current(rollback_position);
          return false;
        }
      }
      if (UpdateRecordLength(current(), rollback_position)) {
        return true;
      }
    }
  } else {
    if (Write<uint16_t>(sizeof(kTXTEmptyRdata)) &&
        Write<uint8_t>(kTXTEmptyRdata)) {
      return true;
    }
  }
  set_current(rollback_position);
  return false;
}

}  // namespace mdns
}  // namespace cast
