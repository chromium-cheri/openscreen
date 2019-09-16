// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TLS_WRITE_BUFFER_H_
#define PLATFORM_API_TLS_WRITE_BUFFER_H_

#include <memory>
#include <mutex>

#include "platform/base/macros.h"

namespace openscreen {
namespace platform {

// This class is responsible for buffering TLS Write data. It is intended to
// allow writing and reading from different threads.
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
    virtual void OnWriteBlocked() = 0;
    virtual void OnWriteUnblocked() = 0;

    // This method signals that, dispite writes being blocked, the sender has
    // continued to send enough data to overwhelm the write buffer.
    virtual void OnTooMuchDataWritten() = 0;
  };

  class ReadInfo {
   public:
    virtual ~ReadInfo() = default;

    // NOTE: MarkRead must only be called on the most recently created ReadInfo
    // object.
    void MarkRead(uint32_t byte_count);

    virtual uint32_t available_bytes() const = 0;
    virtual uint8_t* data_ptr() const = 0;

   protected:
    ReadInfo(TlsWriteBuffer* buffer);

   private:
    TlsWriteBuffer* buffer_;
  };

  virtual ~TlsWriteBuffer();

  static std::unique_ptr<TlsWriteBuffer> Create(Observer* observer);

  // Writes the provided data, calling OnWriteBlocked, OnWriteUnblocked, and
  // OnTooMuchDataWritten as appropriate.
  virtual void Write(const void* data, size_t len) = 0;

  // Returns information about the currently available write data.
  virtual std::unique_ptr<ReadInfo> GetReadable() = 0;

 protected:
  TlsWriteBuffer(Observer* observer);

  virtual void MarkBytesRead(uint32_t byte_count) = 0;

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

  // NOTE: Creating a new ReadInfo object invalidates all previously created
  // ReadInfo objects. This object is expected to outlive any ReadInfo objects
  // that it creates.
  std::unique_ptr<ReadInfo> CreateReadInfo(uint8_t* data_ptr,
                                           int32_t available_bytes);

 private:
  Observer* observer_;

  // Mutex to ensure that MarkRead() can only be called on the most recently
  // created ReadInfo instance.
  std::mutex read_info_mutex_;

  ReadInfo* most_recent_read_info_ = nullptr;

  OSP_DISALLOW_COPY_AND_ASSIGN(TlsWriteBuffer);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_TLS_WRITE_BUFFER_H_
