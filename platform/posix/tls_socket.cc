// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/posix/tls_socket.h"

namespace openscreen {
namespace platform {
namespace {}
// static
ErrorOr<TlsSocketUniquePtr> TlsSocket::Create(IPAddress::Version version) {
  return Error::None();
}

void TlsSocket::Close(CloseReason reason) {
  // auto socket = TlsSocketPosix::From(this);
}

Error TlsSocket::Read() {
  // auto socket = TlsSocketPosix::From(this);
  return Error::None();
}

Error TlsSocket::SendMessage(const TlsSocketMessage& message) {
  // auto socket = TlsSocketPosix::From(this);
  return Error::None();
}
}  // namespace platform
}  // namespace openscreen
