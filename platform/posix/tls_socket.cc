// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/posix/tls_socket.h"

namespace openscreen {
namespace platform {
namespace {

}
  ErrorOr<TlsSocketUniquePtr> TlsSocket::Create(IPAddress::Version version) {
  }

   void TlsSocket::Close(CloseReason reason) {
   }

   Error TlsSocket::Read() {
   }

   Error TlsSocket::SendMessage(const TlsSocketMessage& message) {
   }

   const std::string& TlsSocket::GetFactoryId() const {
   }
}
}
