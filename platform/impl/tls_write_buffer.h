// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_TLS_WRITE_BUFFER_H_
#define PLATFORM_IMPL_TLS_WRITE_BUFFER_H_

#include <memory>

#include "absl/types/span.h"
#include "platform/base/macros.h"

namespace openscreen {
namespace platform {
namespace {
// The amount of space to allocate in the buffer.
constexpr size_t kBufferSize = 1044 * 1024;  // 1 MB space.

// Percentage of the buffer which must be full before OnWriteBufferBlocked()
// signal begin being sent to the observer.
constexpr double kBlockBufferPercentage = 0.5;

// Number of elements which must be present in the queue for
// OnWriteBufferBlocked() to begin being sent to the observer.
constexpr size_t kBeginBlockingBufferCount =
    static_cast<size_t>(kBufferSize * kBlockBufferPercentage);
}  // namespace

// This class is responsible for buffering TLS Write data. The approach taken by
// this class is to allow for a single thread to act as a publisher of data and
// for a separate thread to act as the consumer of that data. The data in
// question is written to a lockless FIFO queue.
// When the underlying array has neared capacity, OnWriteBufferBlocked() signals
// will begin being sent to the observer for each Write call received, and when
// the buffer is no longer near capacity, it will fire OnWriteBufferUnblocked().
// If the buffer ever reaches 100% capacity, OnTooMuchDataBuffered() will fire,
// which is expected to be treated as an unrecoverable error.
class TlsWriteBuffer {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    // These are called to signal that the sender should stop/sending write
    // data, respectively.
    // NOTE: Multiple Write commands may arrive after OnWriteBlocked has been
    // signaled, and implementations of this class are expected to handle this
    // situation.
    virtual void OnWriteBufferBlocked() = 0;
    virtual void OnWriteBufferUnblocked() = 0;

    // This method signals that, dispite writes being blocked, the sender has
    // continued to send enough data to overwhelm the write buffer.
    virtual void OnTooMuchDataBuffered() = 0;
  };

  explicit TlsWriteBuffer(Observer* observer);

  ~TlsWriteBuffer();

  // Writes the provided data, calling OnWriteBlocked, OnWriteUnblocked, and
  // OnTooMuchDataWritten as appropriate.
  void Write(const void* data, size_t len);

  // Returns a subset of the readable region of data. At time of reading, more
  // data may be available for reading than what is represented in this Span.
  absl::Span<const uint8_t> GetReadableRegion();

  // Marks the provided number of bytes as consumed by the consumer thread.
  void ConsumeBytes(size_t byte_count);

 protected:
  // These are called to signal that the sender should stop/sending write data,
  // respectively.
  // NOTE: Multiple Write commands may arrive after OnWriteBlocked has been
  // signaled, and implementations of this class are expected to handle this
  // situation.
  void OnWriteBlocked();
  void OnWriteUnblocked();

  // This method signals that, dispite writes being blocked, the sender has
  // continued to send enough data to overwhelm the write buffer.
  void OnTooMuchDataWritten();

 private:
  Observer* observer_;

  OSP_DISALLOW_COPY_AND_ASSIGN(TlsWriteBuffer);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_TLS_WRITE_BUFFER_H_
