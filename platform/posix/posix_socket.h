// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp_base/error.h"
#include "osp_base/ip_address.h"

namespace openscreen {
namespace platform {
enum class SocketType { Tcp, Udp };
using Version = IPAddress::Version;

class PosixSocket {
 public:
  ErrorOr<PosixSocket> Create(SocketType type, Version version);

  bool operator=(const PosixSocket& other);

  Error SendMessage(const void* data,
                    size_t length,
                    const IPEndpoint& destination) const;

 private:
  PosixSocket(int file_descriptor, Version version);

  const int file_descriptor_;
  Version version_;
};
}  // namespace platform
}  // namespace openscreen