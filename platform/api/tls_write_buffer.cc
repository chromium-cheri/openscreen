// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/tls_write_buffer.h"

#include "platform/api/logging.h"
#include "platform/api/tls_connection.h"

namespace openscreen {
namespace platform {

class ReadInfoImpl : public TlsWriteBuffer::ReadInfo {
 public:
  // NOTE: The provided buffer is expected to outlive this ReadInfo object.
  ReadInfoImpl(TlsWriteBuffer* buffer,
               uint32_t available_bytes,
               uint8_t* data_ptr)
      : ReadInfo(buffer),
        available_bytes_(available_bytes),
        data_ptr_(data_ptr) {}

  ~ReadInfoImpl() override = default;

  uint32_t available_bytes() const override { return available_bytes_; }
  uint8_t* data_ptr() const override { return data_ptr_; }

 private:
  // Number of bytes available to read from buffer_.
  uint32_t available_bytes_;

  // Pointer to bytes in buffer_ which may be read from.
  uint8_t* data_ptr_;
};

std::unique_ptr<TlsWriteBuffer::ReadInfo> TlsWriteBuffer::CreateReadInfo(
    uint8_t* data_ptr,
    int32_t available_bytes) {
  std::lock_guard<std::mutex> lock(read_info_mutex_);
  std::unique_ptr<ReadInfo> read_info =
      std::make_unique<ReadInfoImpl>(this, available_bytes, data_ptr);
  most_recent_read_info_ = read_info.get();
  return read_info;
}

TlsWriteBuffer::ReadInfo::ReadInfo(TlsWriteBuffer* buffer) : buffer_(buffer) {}

TlsWriteBuffer::~TlsWriteBuffer() = default;

void TlsWriteBuffer::ReadInfo::MarkRead(uint32_t byte_count) {
  OSP_DCHECK(byte_count < available_bytes());

  std::lock_guard<std::mutex> lock(buffer_->read_info_mutex_);
  OSP_CHECK(this == buffer_->most_recent_read_info_);
  buffer_->MarkBytesRead(byte_count);
}

TlsWriteBuffer::TlsWriteBuffer(TlsWriteBuffer::Observer* observer)
    : observer_(observer) {}

void TlsWriteBuffer::OnWriteBlocked() {
  if (observer_) {
    observer_->OnWriteBlocked();
  }
}

void TlsWriteBuffer::OnWriteUnblocked() {
  if (observer_) {
    observer_->OnWriteUnblocked();
  }
}

void TlsWriteBuffer::OnTooMuchDataWritten() {
  if (observer_) {
    observer_->OnTooMuchDataWritten();
  }
}

}  // namespace platform
}  // namespace openscreen
