// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_MDNS_MDNS_READER_H_
#define CAST_COMMON_MDNS_MDNS_READER_H_

#include "cast/common/mdns/mdns_rdata.h"
#include "osp_base/big_endian.h"

namespace cast {
namespace mdns {

class MdnsReader : public openscreen::BigEndianReader {
 public:
  MdnsReader(const uint8_t* buffer, size_t length);

  // The following methods return true if the method was able to successfully
  // read the value to |out| and advances current() to point right past the read
  // data. Returns false if the method failed to read the value to |out|,
  // current() remains unchanged.
  bool ReadDomainName(DomainName* out);
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_READER_H_
