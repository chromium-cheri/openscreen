// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/tls_write_buffer.h"

#include "platform/api/logging.h"
#include "platform/api/tls_connection.h"

namespace openscreen {
namespace platform {

std::unique_ptr<TlsWriteBuffer::ReadInfo> TlsWriteBuffer::CreateReadInfo(
    uint8_t** data_ptr,
    int32_t available_bytes) {
  std::lock_guard<std::mutex> lock(read_info_mutex_);
  ReadInfo* read_info = new ReadInfo(this);
  read_info->available_bytes_ = available_bytes;
  read_info->data_ptr_ = data_ptr;
  most_recent_read_info_ = read_info;
  return std::unique_ptr<ReadInfo>(read_info);
}

TlsWriteBuffer::ReadInfo::ReadInfo(TlsWriteBuffer* buffer) : buffer_(buffer) {}

TlsWriteBuffer::~TlsWriteBuffer() = default;

void TlsWriteBuffer::ReadInfo::MarkRead(uint32_t byte_count) {
  OSP_DCHECK(byte_count < available_bytes_);

  std::lock_guard<std::mutex> lock(buffer_->read_info_mutex_);
  OSP_CHECK(this == buffer_->most_recent_read_info_);
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
