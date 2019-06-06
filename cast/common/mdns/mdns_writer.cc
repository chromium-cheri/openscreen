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

}  // namespace

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

}  // namespace mdns
}  // namespace cast
