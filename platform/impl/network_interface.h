// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_NETWORK_INTERFACE_H_
#define PLATFORM_IMPL_NETWORK_INTERFACE_H_

#include <vector>

#include "platform/base/interface_info.h"

namespace openscreen {

enum InterfaceType { kNone = 0, kNonLoopback = 0x1, kLoopback = 0x2 };

// Returns an InterfaceInfo associated with the system's loopback interface.
InterfaceInfo GetLoopbackInterface();

}  // namespace openscreen

#endif  // PLATFORM_IMPL_NETWORK_INTERFACE_H_
