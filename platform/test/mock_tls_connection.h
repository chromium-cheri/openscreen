// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_TEST_MOCK_TLS_CONNECTION_H_
#define PLATFORM_TEST_MOCK_TLS_CONNECTION_H_

#include "gmock/gmock.h"
#include "platform/api/tls_connection.h"

namespace openscreen {
namespace platform {

class MockTlsConnection : public TlsConnection {
 public:
  MockTlsConnection(TaskRunner* task_runner,
                    IPEndpoint local_address,
                    IPEndpoint remote_address)
      : TlsConnection(task_runner),
        local_address_(local_address),
        remote_address_(remote_address) {}

  ~MockTlsConnection() override = default;

  MOCK_METHOD(void, Write, (const void* data, size_t len));

  IPEndpoint local_address() const override { return local_address_; }
  IPEndpoint remote_address() const override { return remote_address_; }

  void OnWriteBlocked() { TlsConnection::OnWriteBlocked(); }
  void OnWriteUnblocked() { TlsConnection::OnWriteUnblocked(); }
  void OnError(Error error) { TlsConnection::OnError(error); }
  void OnRead(std::vector<uint8_t> block) { TlsConnection::OnRead(block); }

 private:
  const IPEndpoint local_address_;
  const IPEndpoint remote_address_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_TEST_MOCK_TLS_CONNECTION_H_
