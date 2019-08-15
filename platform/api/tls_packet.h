// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TLS_PACKET_H_
#define PLATFORM_API_TLS_PACKET_H_

#include <cstdint>
#include <memory>
#include <string>

#include "absl/types/optional.h"
#include "platform/api/network_interface.h"
#include "platform/api/tls_socket_creds.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"
#include "platform/base/macros.h"

namespace openscreen {
namespace platform {

struct TlsPacket {
  void* data;
  size_t length;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_TLS_PACKET_H_
