// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/test/mock_tls_connection.h"

namespace openscreen {

MockTlsConnection::MockTlsConnection(IPEndpoint local_address,
                                     IPEndpoint remote_address)
    : local_address_(local_address), remote_address_(remote_address) {}

MockTlsConnection::~MockTlsConnection() = default;

}  // namespace openscreen
