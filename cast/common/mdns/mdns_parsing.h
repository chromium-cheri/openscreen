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
#include "osp_base/macros.h"

namespace cast {
namespace mdns {

class DomainName {
 public:
  DomainName();
  DomainName(const DomainName& other);
  ~DomainName();

  bool IsEqual(const DomainName& other) const;
  bool operator==(const DomainName& other) const;
  DomainName& operator=(const DomainName& other);

  void Clear();
  bool PushLabel(absl::string_view label);

  // Gets the absl::string_view for the underlying domain name starting from a
  // given label index. This is used during name compression to map sub domain
  // name suffixes for compression.
  //
  // If |label_index| is greater than or equal to the number of labels in the
  // domain name, the empty string will be returned.
  //
  // Example:
  //   name = "mydevice._googlecast._tcp.local"
  //   SubName(0) = "mydevice._googlecast._tcp.local"
  //   SubName(1) = "_googlecast._tcp.local"
  //   SubName(2) = "_tcp.local"
  //   SubName(3) = "local"
  //   SubName(4) = ""
  absl::string_view SubName(size_t label_index) const;
  // Computes the hash for the underlying domain name starting from a given
  // label index, i.e. compute the case-insensitive hash for the result of
  // SubName(). This is a quick way to identify if sub-names could be equal.
  // This is used during name compression as the key for mapping previous
  // domain names to their packet offsets.
  size_t SubHash(size_t label_index) const;

  // Returns the maximum space that the domain name will take up in its
  // on-the-wire format. NOTE: We must add a byte for the terminating length.
  size_t max_wire_size() const { return wire_size_ + 1; }
  bool empty() const { return labels_.empty(); }
  const std::string& as_string() const { return name_; }
  // Returns in-order list of domain name labels (delimiters are not included).
  // The labels themselves are just views into the underlying backing string,
  // so do not use absl::string_view like a string in all cases.
  const std::vector<absl::string_view>& labels() const { return labels_; }
  // Returns in-order list of hash values for each corresponding label in the
  // name. The hashes are not perfect, so labels must still be checked for
  // true equality.
  const std::vector<size_t>& hashes() const { return hashes_; }

 private:
  void CopyFrom(const DomainName& other);

  size_t wire_size_;
  std::vector<absl::string_view> labels_;
  std::vector<size_t> hashes_;
  std::string name_;
};

// Helper function to print readable domain name to stream.
std::ostream& operator<<(std::ostream& stream, const DomainName& domain_name);

// Allows reading integers in network order (big endian) while iterating over
// an underlying buffer. All the reading functions advance the internal pointer.
class BigEndianReader {
 public:
  BigEndianReader(const uint8_t* buf, size_t len);

  const uint8_t* ptr() const { return ptr_; }
  size_t remaining() const { return end_ - ptr_; }

  bool Skip(size_t len);
  bool ReadBytes(void* out, size_t len);
  // Creates a absl::string_view in |out| that points to the underlying buffer.
  bool ReadPiece(absl::string_view* out, size_t len);
  bool ReadU8(uint8_t* value);
  bool ReadU16(uint16_t* value);
  bool ReadU32(uint32_t* value);
  bool ReadU64(uint64_t* value);

 private:
  // Hidden to promote type safety.
  template <typename T>
  bool Read(T* v);

  const uint8_t* ptr_;
  const uint8_t* end_;
};

// Allows writing integers in network order (big endian) while iterating over
// an underlying buffer. All the writing functions advance the internal pointer.
class BigEndianWriter {
 public:
  BigEndianWriter(uint8_t* buf, size_t len);

  uint8_t* ptr() const { return ptr_; }
  size_t remaining() const { return end_ - ptr_; }

  bool Skip(size_t len);
  bool WriteBytes(const void* buf, size_t len);
  bool WriteU8(uint8_t value);
  bool WriteU16(uint16_t value);
  bool WriteU32(uint32_t value);
  bool WriteU64(uint64_t value);

 private:
  // Hidden to promote type safety.
  template <typename T>
  bool Write(T v);

  uint8_t* ptr_;
  uint8_t* end_;
};

class MdnsReader : public BigEndianReader {
 public:
  MdnsReader(const uint8_t* buffer, size_t length);
  ~MdnsReader();

  bool ReadDomainName(DomainName* name);

  const uint8_t* buffer() { return buffer_; }
  size_t length() { return length_; }
  size_t offset() { return ptr() - buffer_; }

 private:
  const uint8_t* buffer_;
  size_t length_;

  OSP_DISALLOW_COPY_AND_ASSIGN(MdnsReader);
};

class MdnsWriter : public BigEndianWriter {
 public:
  MdnsWriter(uint8_t* buffer, size_t length);
  ~MdnsWriter();

  bool WriteDomainName(const DomainName& name, bool allow_compression);

  uint8_t* buffer() { return buffer_; }
  size_t length() { return length_; }
  size_t offset() { return ptr() - buffer_; }

 private:
  uint8_t* buffer_;
  size_t length_;
  // Mapping of domain names that have previously been written in the packet.
  std::unordered_map<size_t /* sub-hash */, uint16_t /* offset */>
      compression_map_;

  OSP_DISALLOW_COPY_AND_ASSIGN(MdnsWriter);
};

bool IsValidDomainLabel(const std::string& label);

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_PARSING_H_
