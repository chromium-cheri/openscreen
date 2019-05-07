// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_parsing.h"

#include "absl/hash/hash.h"
#include "absl/strings/ascii.h"
#include "cast/common/mdns/mdns_constants.h"
#include "osp_base/big_endian.h"
#include "platform/api/logging.h"

// A shortcut macro that will log a message and return false if an operation
// fails, or continue executing if it succeeds.
#define MAKE_SURE(expression)                           \
  do {                                                  \
    if (!(expression)) {                                \
      OSP_DVLOG << "MAKE_SURE failed: " << #expression; \
      return false;                                     \
    }                                                   \
  } while (0);

namespace cast {
namespace mdns {

namespace {

// We use our own comparison function that goes character by character in each
// string since base::strcasecmp() will incorrectly return true if two strings
// match up to the first NULL char. Since it is possible for '\0' to be
// specified within a domain name label we must check every character and not
// stop when we hit the first NULL.
bool CompareLabel(absl::string_view s1, absl::string_view s2) {
  if (s1.size() != s2.size()) {
    return false;
  }
  for (size_t i = 0; i < s1.size(); ++i) {
    if (absl::ascii_tolower(s1[i]) != absl::ascii_tolower(s2[i])) {
      return false;
    }
  }
  return true;
}

// Each label in |label_list| is rebuilt using the new underlying data buffer.
// The offsets and the lengths are the same, but the base pointer for the
// absl::string_view instances need to be fixed up to point to |new_base_|
// instead of |old_base_|.
//
// NOTE: If |old_base_| == |new_base_| nothing will change.
void RebuildLabels(const char* new_base_,
                   const char* old_base_,
                   std::vector<absl::string_view>* label_list) {
  OSP_DCHECK(label_list);
  OSP_DCHECK(old_base_);
  OSP_DCHECK(new_base_);
  for (absl::string_view& label : *label_list) {
    OSP_DCHECK_GE(label.data(), old_base_);
    size_t offset = label.data() - old_base_;
    size_t length = label.size();
    label = absl::string_view(new_base_ + offset, length);
  }
}

// Helper class that iterates all the labels in a domain name, given a
// starting position inside some memory buffer. The labels will be parsed one
// at a time according to the DNS spec for domain names. No dynamic memory
// allocations will be made by iterator, it just parses absl::string_view label
// from underlying buffer.
class LabelIterator {
 public:
  LabelIterator(const uint8_t* begin,
                const uint8_t* end,
                const uint8_t* current)
      : begin_(begin),
        end_(end),
        start_(current),
        current_(current),
        error_(false),
        finished_(false),
        seen_(0),
        consumed_(0) {
    OSP_DCHECK(begin);
    OSP_DCHECK(end);
    OSP_DCHECK(current);
  }

  // Number of bytes consumed reading from the starting position to either the
  // first jump or the final termination byte. This should correspond with the
  // number of consecutive bytes the domain name took up.
  size_t consumed() const { return consumed_; }
  // Set to true once the final terminate label is reached and the domain name
  // is done parsing. All further calls to NextLabel() will return false.
  bool finished() const { return finished_; }
  // Set to true whenever an error occurs while parsing. All further calls to
  // NextLabel() will return false.
  bool error() const { return error_; }
  // The most recent label parsed from buffer corresponding to most recent
  // call to NextLabel(). If an error occurs or name is done parsing the
  // label will be empty.
  absl::string_view label() const { return label_; }

  // Returns true if a new label was successfully parsed and set. Returns false
  // if an error occurred or reached the end of the domain name. Will set the
  // |finished| flag if the end of the name is reached, and will set |error|
  // if an error occurs.
  bool NextLabel() {
    label_ = absl::string_view();
    if (error_ || finished_)
      return false;
    for (;;) {
      if (current_ >= end_) {
        error_ = true;
        return false;
      }
      switch (*current_ & kLabelMask) {
        case kLabelPointer: {
          if (current_ + sizeof(uint16_t) > end_) {
            error_ = true;
            return false;
          }
          if (consumed_ == 0) {
            consumed_ = current_ - start_ + sizeof(uint16_t);
          }
          seen_ += sizeof(uint16_t);
          // if seen greater than packet length, then we are in a loop.
          size_t buffer_length = end_ - begin_;
          if (seen_ > buffer_length) {
            error_ = true;
            return false;
          }
          uint16_t label_offset = openscreen::ReadBigEndian<uint16_t>(current_);
          label_offset &= kLabelOffsetMask;
          current_ = begin_ + label_offset;
          if (current_ >= end_) {
            error_ = true;
            return false;
          }
          // Break out of switch to attempt to parse the next direct label
          break;
        }
        case kLabelDirect: {
          uint8_t label_length = *current_;
          ++current_;
          // NOTE: root domain (".") is NOT included.
          if (label_length == 0) {
            if (consumed_ == 0) {
              consumed_ = current_ - start_;
            }  // else we set |consumed| before first jump
            // We are finished reading full domain name. Return success.
            finished_ = true;
            return false;
          }
          if (current_ + label_length >= end_) {
            error_ = true;
            return false;
          }
          label_ = absl::string_view(reinterpret_cast<const char*>(current_),
                                     label_length);
          current_ += label_length;
          seen_ += 1 + label_length;
          return true;
        }
        default:
          error_ = true;
          return false;
      }
    }
  }

 private:
  const uint8_t* const begin_;
  const uint8_t* const end_;
  const uint8_t* const start_;
  const uint8_t* current_;
  bool error_;
  bool finished_;
  size_t seen_;
  size_t consumed_;
  absl::string_view label_;

  OSP_DISALLOW_COPY_AND_ASSIGN(LabelIterator);
};

}  // namespace

DomainName::DomainName() : wire_size_(0) {}

DomainName::DomainName(const DomainName& other) {
  CopyFrom(other);
}

DomainName::~DomainName() = default;

void DomainName::Clear() {
  wire_size_ = 0;
  labels_.clear();
  hashes_.clear();
  name_.clear();
}

bool DomainName::PushLabel(absl::string_view label) {
  if (label.empty() || label.size() > kMaxLabelLength) {
    OSP_LOG_ERROR << "Invalid domain name label: \"" << label << "\"";
    return false;
  }
  // Don't forget to include the length byte in the size calculation.
  if (wire_size_ + label.size() + 1 > kMaxDomainNameLength) {
    OSP_LOG_ERROR << "Name too long. Cannot push label: \"" << label << "\"";
    return false;
  }
  // Unfortunately as we append new label to |name_|, the std::string may
  // reallocate into a larger buffer that invalidates all the data() pointers
  // for the cached label absl::string_view instances. Store the previous data()
  // pointer, and use that after the append to fix up absl::string_view
  // pointers.
  const char* old_name_data = name_.data();
  // Add the label to the underlying domain name string, making sure to add
  // delimiter for all labels after the first.
  if (!name_.empty())
    name_.push_back('.');
  name_.append(label.data(), label.size());
  // Fix all old label absl::string_view instances to point to new |name_| in
  // memory. The label offsets and lengths remain the same, just shift base
  // pointer.
  RebuildLabels(name_.data(), old_name_data, &labels_);

  // Grab the last N bytes corresponding to the newly added label and create
  // a absl::string_view slice of the underlying string. absl::string_view
  // doesn't perform any allocations and is just a view of the underlying
  // buffer.
  size_t offset = name_.size() - label.size();
  absl::string_view new_label(name_.data() + offset, name_.size() - offset);
  labels_.push_back(new_label);
  // Calculate the hash of the newly added label and append it to hash list.
  // This list is used during name compression to build the compression
  // mapping as well as speed up domain name equality checks. Convert the label
  // to lower-case first to make sure the hash is case-insensitive.
  std::string label_lower(absl::AsciiStrToLower(new_label));
  hashes_.push_back(absl::Hash<std::string>{}(label_lower));

  // Update the size of the full name in wire format. Don't forget to include
  // the length byte in the size calculation.
  wire_size_ += label.size() + 1;
  return true;
}

absl::string_view DomainName::SubName(size_t label_index) const {
  if (label_index >= labels_.size())
    return absl::string_view();
  size_t offset = labels_[label_index].data() - name_.data();
  return absl::string_view(name_.data() + offset, name_.size() - offset);
}

size_t DomainName::SubHash(size_t label_index) const {
  if (label_index >= hashes_.size())
    return 0;
  // Grab a view of the underlying subname beginning at the specified label, and
  // hash the lower-case string. This should fairly uniquely represent a given
  // subname useful for matching compression targets. Converting to lower-case
  // is required to make the hash case insensitive.
  //
  // NOTE: Hashing the string representation has the one downside that certain
  // label combinations have different labels and label hashes, but the same
  // subname hash. This isn't a big deal since it should rarely happen, and if
  // it does the compression logic should catch it.
  //    [8]MyDevice[4]_udp[5]local -> "MyDevice._udp.local"
  //    [13]MyDevice._udp[5]local -> "MyDevice._udp.local"
  std::string subname = absl::AsciiStrToLower(SubName(label_index));
  return absl::Hash<std::string>{}(subname);
}

bool DomainName::IsEqual(const DomainName& other) const {
  // First check the label hashes to avoid string comparisons if possible. Since
  // all labels are converted to lower case when generating the hashes, this
  // should be a case-insensitive comparison.
  if (hashes_ != other.hashes_)
    return false;
  // If the hashes match a more expensive string comparison needs to occur to
  // verify the strings are exactly the same (cast-insensitive).
  if (labels_.size() != other.labels_.size())
    return false;
  for (size_t i = 0; i < labels_.size(); ++i) {
    if (!CompareLabel(labels_[i], other.labels_[i]))
      return false;
  }
  return true;
}

bool DomainName::operator==(const DomainName& other) const {
  return IsEqual(other);
}

DomainName& DomainName::operator=(const DomainName& other) {
  CopyFrom(other);
  return *this;
}

void DomainName::CopyFrom(const DomainName& other) {
  wire_size_ = other.wire_size_;
  hashes_ = other.hashes_;
  name_ = other.name_;
  // The |labels_| field needs to be rebuilt using the new underlying data. The
  // offsets and the lengths are the same, but the base pointer for the
  // absl::string_view instances need to be fixed up to point to this instance's
  // |name_| field instead of |other|.
  labels_ = other.labels_;
  RebuildLabels(name_.data(), other.name_.data(), &labels_);
}

std::ostream& operator<<(std::ostream& stream, const DomainName& domain_name) {
  return stream << domain_name.as_string();
}

BigEndianReader::BigEndianReader(const uint8_t* buf, size_t len)
    : ptr_(buf), end_(ptr_ + len) {}

bool BigEndianReader::Skip(size_t len) {
  if (ptr_ + len > end_)
    return false;
  ptr_ += len;
  return true;
}

bool BigEndianReader::ReadBytes(void* out, size_t len) {
  if (ptr_ + len > end_)
    return false;
  memcpy(out, ptr_, len);
  ptr_ += len;
  return true;
}

bool BigEndianReader::ReadPiece(absl::string_view* out, size_t len) {
  if (ptr_ + len > end_)
    return false;
  *out = absl::string_view(reinterpret_cast<const char*>(ptr_), len);
  ptr_ += len;
  return true;
}

template <typename T>
bool BigEndianReader::Read(T* value) {
  if (ptr_ + sizeof(T) > end_)
    return false;
  *value = openscreen::ReadBigEndian<T>(ptr_);
  ptr_ += sizeof(T);
  return true;
}

bool BigEndianReader::ReadU8(uint8_t* value) {
  return Read(value);
}

bool BigEndianReader::ReadU16(uint16_t* value) {
  return Read(value);
}

bool BigEndianReader::ReadU32(uint32_t* value) {
  return Read(value);
}

bool BigEndianReader::ReadU64(uint64_t* value) {
  return Read(value);
}

BigEndianWriter::BigEndianWriter(uint8_t* buf, size_t len)
    : ptr_(buf), end_(ptr_ + len) {}

bool BigEndianWriter::Skip(size_t len) {
  if (ptr_ + len > end_) {
    return false;
  }
  ptr_ += len;
  return true;
}

bool BigEndianWriter::WriteBytes(const void* buf, size_t len) {
  if (ptr_ + len > end_) {
    return false;
  }
  memcpy(ptr_, buf, len);
  ptr_ += len;
  return true;
}

template <typename T>
bool BigEndianWriter::Write(T value) {
  if (ptr_ + sizeof(T) > end_) {
    return false;
  }
  openscreen::WriteBigEndian<T>(value, ptr_);
  ptr_ += sizeof(T);
  return true;
}

bool BigEndianWriter::WriteU8(uint8_t value) {
  return Write(value);
}

bool BigEndianWriter::WriteU16(uint16_t value) {
  return Write(value);
}

bool BigEndianWriter::WriteU32(uint32_t value) {
  return Write(value);
}

bool BigEndianWriter::WriteU64(uint64_t value) {
  return Write(value);
}

MdnsReader::MdnsReader(const uint8_t* buffer, size_t length)
    : BigEndianReader(buffer, length), buffer_(buffer), length_(length) {}

MdnsReader::~MdnsReader() = default;

bool MdnsReader::ReadDomainName(DomainName* name) {
  OSP_DCHECK(name);
  OSP_DCHECK(buffer_);
  name->Clear();
  // Iterate over each label in the buffer and push it to |name|.
  LabelIterator it(buffer_, buffer_ + length_, ptr());
  while (it.NextLabel()) {
    OSP_DCHECK(!it.label().empty());
    if (!name->PushLabel(it.label()))
      return false;
  }
  // Iterator should either have finished or encountered an error. If neither
  // then return false.
  if (it.error() || !it.finished())
    return false;
  // Iterator finished parsing name and there were no errors. Move the reader
  // forward by the number of bytes consumed by the full domain name. This takes
  // into account pointer labels.
  return Skip(it.consumed());
}

MdnsWriter::MdnsWriter(uint8_t* buffer, size_t length)
    : BigEndianWriter(buffer, length), buffer_(buffer), length_(length) {}

MdnsWriter::~MdnsWriter() = default;

bool MdnsWriter::WriteDomainName(const DomainName& name,
                                 bool allow_compression) {
  if (name.labels().empty())
    return false;
  // Leave an extra byte for the terminating length byte.
  std::unordered_map<size_t, uint16_t> local_map;
  char local_buf[kMaxDomainNameLength + 1];
  size_t offset = 0;
  for (size_t i = 0; i < name.labels().size(); ++i) {
    absl::string_view label = name.labels().at(i);
    if (label.empty() || label.size() > kMaxLabelLength)
      return false;
    // Check if the current sub-name has already been written in the packet.
    // If so, use a pointer label rather than direct label.
    if (allow_compression) {
      size_t subhash = name.SubHash(i);
      auto it = compression_map_.find(subhash);
      if (it != compression_map_.end()) {
        // A matching hash was found in map, need to confirm the names exactly
        // match. Iterate over the labels in the current buffer and check they
        // match the current labels to be written. The map stores the offset
        // of the compressed name from the beginning of the buffer.
        //
        // NOTE: There should rarely be a hash collision where labels do not
        // match, so this check should almost always succeed, so the overhead
        // doesn't hit performance too much.
        auto subname_it = name.labels().begin() + i;
        LabelIterator compressed_it(buffer_, buffer_ + length_,
                                    buffer_ + it->second);
        while (compressed_it.NextLabel()) {
          if (subname_it == name.labels().end())
            break;
          if (!CompareLabel(*subname_it, compressed_it.label()))
            break;
          ++subname_it;
        }
        // If domain name finished, no errors, and label also reached end at
        // same time, then must have been a match. If no match, continue
        // processing like normal.
        if (!compressed_it.error() && compressed_it.finished() &&
            subname_it == name.labels().end()) {
          uint16_t pointer_offset = it->second & kLabelOffsetMask;
          OSP_DVLOG << "Compressing MDNS name: " << pointer_offset;
          local_buf[offset++] = kLabelPointer | (pointer_offset >> 8);
          local_buf[offset++] = (pointer_offset & 0xFF);  // Lower byte
          MAKE_SURE(WriteBytes(local_buf, offset));
          compression_map_.insert(local_map.begin(), local_map.end());
          return true;
        }
      }
      // No match was found, but save the current sub-name into the local
      // compression mapping for later domain names to maybe use.
      uint16_t packet_offset = this->offset() + offset;
      packet_offset &= kLabelOffsetMask;
      local_map.emplace(subhash, packet_offset);
    }
    // Check if there is enough room to write the direct label into the buffer,
    // making sure to include the length byte.
    if (offset + label.size() + 1 > sizeof(local_buf))
      return false;
    local_buf[offset++] = label.size();
    memcpy(local_buf + offset, label.data(), label.size());
    offset += label.size();
  }
  if (offset + 1 > sizeof(local_buf))
    return false;
  local_buf[offset++] = kLabelTermination;
  MAKE_SURE(WriteBytes(local_buf, offset));
  compression_map_.insert(local_map.begin(), local_map.end());
  return true;
}

bool IsValidDomainLabel(const std::string& label) {
  const size_t label_size = label.size();
  return label_size > 0 && label_size <= kMaxLabelLength;
}

}  // namespace mdns
}  // namespace cast
