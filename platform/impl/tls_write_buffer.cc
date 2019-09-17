// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/tls_write_buffer.h"

#include "platform/api/logging.h"
#include "platform/api/tls_connection.h"

namespace openscreen {
namespace platform {

TlsWriteBuffer::~TlsWriteBuffer() = default;

TlsWriteBuffer::TlsWriteBuffer(TlsWriteBuffer::Observer* observer)
    : observer_(observer) {}

void TlsWriteBuffer::Write(const void* data, size_t len) {
  // TODO(rwkeane): Implement this method.
  OSP_UNIMPLEMENTED();
}

absl::Span<const uint8_t> TlsWriteBuffer::GetReadableRegion() {
  // TODO(rwkeane): Implement this method.
  OSP_UNIMPLEMENTED();
  return absl::Span<const uint8_t>();
}

void TlsWriteBuffer::ConsumeBytes(size_t byte_count) {
  // TODO(rwkeane): Implement this method.
  OSP_UNIMPLEMENTED();
}

void TlsWriteBuffer::OnWriteBlocked() {
  if (observer_) {
    observer_->OnWriteBufferBlocked();
  }
}

void TlsWriteBuffer::OnWriteUnblocked() {
  if (observer_) {
    observer_->OnWriteBufferUnblocked();
  }
}

void TlsWriteBuffer::OnTooMuchDataWritten() {
  if (observer_) {
    observer_->OnTooMuchDataBuffered();
  }
}

}  // namespace platform
}  // namespace openscreen
