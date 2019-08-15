// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_TLS_SOCKET_POSIX_H_
#define PLATFORM_IMPL_TLS_SOCKET_POSIX_H_

#include "platform/api/tls_socket.h"

#include <string>

namespace openscreen {
namespace platform {

struct TlsSocketPosix : public TlsSocket {
public:
  // Tls socket overrides
  bool IsIPv4() const override;
  bool IsIPv6() const override;
  void Close(CloseReason reason) override;
  Error Read() override;
  Error Write(const TlsSocketMessage& message) override;
  const std::string& GetFactoryId() const override;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_TLS_SOCKET_POSIX_H_
