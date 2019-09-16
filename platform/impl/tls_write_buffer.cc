// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/tls_write_buffer.h"

namespace openscreen {
namespace platform {

// static
std::unique_ptr<TlsWriteBuffer> TlsWriteBuffer::Create(
    TlsWriteBuffer::Observer* observer) {
  return std::unique_ptr<TlsWriteBuffer>();
}

}  // namespace platform
}  // namespace openscreen
