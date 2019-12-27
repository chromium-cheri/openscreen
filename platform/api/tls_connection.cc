// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/tls_connection.h"

#include <cassert>

namespace openscreen {

TlsConnection::TlsConnection() = default;
TlsConnection::~TlsConnection() = default;

// TODO(crbug/openscreen/80): Remove after Chromium is migrated to Send().
void TlsConnection::Write(const void* data, size_t len) {
  assert(false);  // Not reached.
}

}  // namespace openscreen
