// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/tls_write_buffer.h"

#include "platform/api/logging.h"
#include "platform/api/tls_connection.h"

namespace openscreen {
namespace platform {

TlsWriteBuffer::ReadInfo::ReadInfo(TlsWriteBuffer* buffer) : buffer_(buffer) {}

TlsWriteBuffer::~TlsWriteBuffer() = default;

void TlsWriteBuffer::ReadInfo::MarkRead(uint32_t byte_count) {
  OSP_DCHECK(byte_count < available_bytes);
  buffer_->MarkBytesRead(byte_count);
}

TlsWriteBuffer::TlsWriteBuffer(TlsConnection* connection)
    : connection_(connection) {}

void TlsWriteBuffer::OnWriteBlocked() {
  connection_->OnWriteBlocked();
}

void TlsWriteBuffer::OnWriteUnblocked() {
  connection_->OnWriteUnblocked();
}

void TlsWriteBuffer::OnTooMuchDataWritten() {
  connection_->OnError(Error::Code::kInsufficientBuffer);
}

}  // namespace platform
}  // namespace openscreen
