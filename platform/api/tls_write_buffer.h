// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TLS_WRITE_BUFFER_H_
#define PLATFORM_API_TLS_WRITE_BUFFER_H_

#include <memory>

namespace openscreen {
namespace platform {

class TlsConnection;

// This class is responsible for buffering TLS Write data. It is intended to
// allow writing and reading from different threads.
class TlsWriteBuffer {
 public:
  struct ReadInfo {
   public:
    ReadInfo(TlsWriteBuffer* buffer);

    void MarkRead(uint32_t byte_count);

    uint32_t available_bytes;
    uint8_t** data_ptr;

   private:
    TlsWriteBuffer* buffer_;
  };

  virtual ~TlsWriteBuffer();

  static std::unique_ptr<TlsWriteBuffer> Create(TlsConnection* buffer);

  // Writes the provided data, calling OnWriteBlocked, OnWriteUnblocked, and
  // OnTooMuchDataWritten as appropriate.
  virtual void Write(const void* data, size_t len) = 0;

  // Returns information about the currently available write data.
  virtual ReadInfo GetReadable() = 0;

 protected:
  TlsWriteBuffer(TlsConnection* connection);

  virtual void MarkBytesRead(uint32_t byte_count) = 0;

  void OnWriteBlocked();
  void OnWriteUnblocked();
  void OnTooMuchDataWritten();

 private:
  TlsConnection* connection_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_TLS_WRITE_BUFFER_H_
