// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_UTILS_H_
#define DISCOVERY_MDNS_MDNS_UTILS_H_

#include <sstream>
#include <string>

#include "discovery/mdns/mdns_records.h"

namespace openscreen {
namespace discovery {

std::string GetRecordLog(const MdnsRecord& record);

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_MDNS_UTILS_H_
