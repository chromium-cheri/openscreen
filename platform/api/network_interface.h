// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_NETWORK_INTERFACE_H_
#define PLATFORM_API_NETWORK_INTERFACE_H_

#include <vector>

#include "platform/base/network_interface_structs.h"

namespace openscreen {
namespace platform {

// Returns an InterfaceInfo for each currently active network interface on the
// system. No two entries in this vector can have the same NetworkInterfaceIndex
// value.
std::vector<InterfaceInfo> GetNetworkInterfaces();

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_NETWORK_INTERFACE_H_
