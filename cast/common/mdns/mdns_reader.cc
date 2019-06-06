// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_reader.h"

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

}  // namespace mdns
}  // namespace cast
