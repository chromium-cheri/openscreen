// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_OPAQUE_CONVERSIONS_H_
#define DISCOVERY_MDNS_OPAQUE_CONVERSIONS_H_

#include <cstring>

namespace openscreen {
namespace mdns {

// Convert from a pointer to an opaque identifier type safely.
template <typename Pointer, typename OpaqueId>
void ConvertToOpaqueId(Pointer pointer, OpaqueId* dest) {
  static_assert(sizeof(OpaqueId) >= sizeof(Pointer),
                "OpaqueId is too small to store Pointer");
  memcpy(dest, &pointer, sizeof(Pointer));
  if (sizeof(OpaqueId) > sizeof(Pointer)) {
    memset(reinterpret_cast<uint8_t*>(dest) + sizeof(Pointer), 0,
           sizeof(OpaqueId) - sizeof(Pointer));
  }
}

// Convert from an opaque identifier back to a pointer safely.
template <typename OpaqueId, typename Pointer>
void ConvertToPointer(const OpaqueId* src, Pointer* dest) {
  static_assert(sizeof(OpaqueId) >= sizeof(Pointer),
                "OpaqueId is too small to retrieve a valid Pointer");
  memcpy(dest, src, sizeof(Pointer));
}

}  // namespace mdns
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_OPAQUE_CONVERSIONS_H_
