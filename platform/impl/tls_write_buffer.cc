// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/tls_write_buffer.h"

#include <algorithm>
#include <cstring>

#include "platform/api/logging.h"
#include "platform/api/tls_connection.h"

namespace openscreen {
namespace platform {
namespace {
inline size_t GetCurrentFillCount(size_t write_index, size_t read_index) {
  return (write_index + TlsWriteBuffer::kBufferSizeBytes - read_index) %
         TlsWriteBuffer::kBufferSizeBytes;
}
}  // namespace

TlsWriteBuffer::~TlsWriteBuffer() = default;

TlsWriteBuffer::TlsWriteBuffer(TlsWriteBuffer::Observer* observer)
    : observer_(observer) {
  OSP_DCHECK(observer_);
}

size_t TlsWriteBuffer::Write(const void* data, size_t len) {
  const size_t current_write_index = write_index_.load();
  const size_t current_read_index = read_index_.load();

  // Calculates the current size of the buffer.
  const size_t current_size =
      GetCurrentFillCount(current_write_index, current_read_index);

  // Calculates how many bytes out of the requested |len| can be written without
  // causing a buffer overflow.
  const size_t write_len = std::min(kBufferSizeBytes - current_size - 1, len);

  // Calculates the number of bytes out of |write_len| to write in the first
  // memcpy operation, which is either all |write_len| or the number that can be
  // written before wrapping around to the beginning of the underlying array.
  const size_t first_write_len =
      std::min(write_len, kBufferSizeBytes - current_write_index);
  memcpy(&buffer_[current_write_index], data, first_write_len);

  // If we didn't write all |write_len| bytes in the previous memcpy, copy any
  // remaining bytes to the array, starting at 0 (since the last write must have
  // finished at the end of the array).
  if (first_write_len != write_len) {
    const uint8_t* new_start =
        static_cast<const uint8_t*>(data) + first_write_len;
    memcpy(buffer_, new_start, write_len - first_write_len);
  }

  // Store and return updated values.
  const size_t new_write_index =
      (current_write_index + write_len) % kBufferSizeBytes;
  write_index_.store(new_write_index);
  NotifyWriteBufferFill(new_write_index, current_read_index);
  return write_len;
}

absl::Span<const uint8_t> TlsWriteBuffer::GetReadableRegion() {
  const size_t current_write_index = write_index_.load();
  const size_t current_read_index = read_index_.load();

  // Stop reading at either the end of the array or the current write index,
  // whichever is sooner. While there may be more data wrapped around after the
  // end of the array, the API for GetReadableRegion() only guarantees to return
  // a subset of all available read data, so there is no reason to introduce
  // this additional level of complexity.
  const size_t end_index = current_write_index >= current_read_index
                               ? current_write_index
                               : kBufferSizeBytes - 1;
  return absl::Span<const uint8_t>(&buffer_[current_read_index],
                                   end_index - current_read_index);
}

void TlsWriteBuffer::Consume(size_t byte_count) {
  const size_t current_write_index = write_index_.load();
  const size_t current_read_index = read_index_.load();

  OSP_CHECK(GetCurrentFillCount(current_write_index, current_read_index) >=
            byte_count);
  const size_t new_read_index =
      (current_read_index + byte_count) % kBufferSizeBytes;
  read_index_.store(new_read_index);

  NotifyWriteBufferFill(current_write_index, new_read_index);
}

void TlsWriteBuffer::NotifyWriteBufferFill(size_t write_index,
                                           size_t read_index) {
  double fraction =
      static_cast<double>(GetCurrentFillCount(write_index, read_index)) /
      kBufferSizeBytes;
  observer_->NotifyWriteBufferFill(fraction);
}

}  // namespace platform
}  // namespace openscreen
