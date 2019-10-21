// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/channel/virtual_connection.h"

namespace cast {
namespace channel {

bool operator==(const VirtualConnection& a, const VirtualConnection& b) {
  return a.local_id == b.local_id && a.peer_id == b.peer_id &&
         a.socket_id == b.socket_id;
}

bool operator!=(const VirtualConnection& a, const VirtualConnection& b) {
  return !(a == b);
}

}  // namespace channel
}  // namespace cast
