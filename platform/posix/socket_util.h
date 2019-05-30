// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp_base/error.h"

#include "osp_base/ip_address.h"

namespace openscreen {
namespace platform {
enum class SocketType {
  Tcp,
  Udp
};

ErrorOr<int> CreateNonBlockingSocket(SocketType type, IPAddress::Version version);


}
}