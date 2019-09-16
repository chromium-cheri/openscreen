// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/tls_connection.h"

#include "platform/api/task_runner.h"

namespace openscreen {
namespace platform {

TlsConnection::TlsConnection(TaskRunner* task_runner)
    : task_runner_(task_runner), write_buffer_(TlsWriteBuffer::Create(this)) {}

TlsConnection::~TlsConnection() = default;

void TlsConnection::OnWriteBlocked() {
  if (!client_) {
    return;
  }

  task_runner_->PostTask([this]() {
    // TODO(issues/71): |this| may be invalid at this point.
    this->client_->OnWriteBlocked(this);
  });
}

void TlsConnection::OnWriteUnblocked() {
  if (!client_) {
    return;
  }

  task_runner_->PostTask([this]() {
    // TODO(issues/71): |this| may be invalid at this point.
    this->client_->OnWriteUnblocked(this);
  });
}

void TlsConnection::OnTooMuchDataWritten() {
  OnError(Error::Code::kInsufficientBuffer);
}

void TlsConnection::OnError(Error error) {
  if (!client_) {
    return;
  }

  task_runner_->PostTask([e = std::move(error), this]() mutable {
    // TODO(issues/71): |this| may be invalid at this point.
    this->client_->OnError(this, std::move(e));
  });
}

void TlsConnection::OnRead(std::vector<uint8_t> message) {
  if (!client_) {
    return;
  }

  task_runner_->PostTask([m = std::move(message), this]() mutable {
    // TODO(issues/71): |this| may be invalid at this point.
    this->client_->OnRead(this, std::move(m));
  });
}

void TlsConnection::Write(const void* data, size_t len) {
  write_buffer_->Write(data, len);
}

}  // namespace platform
}  // namespace openscreen
