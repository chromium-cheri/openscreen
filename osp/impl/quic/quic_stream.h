// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_STREAM_H_
#define OSP_IMPL_QUIC_QUIC_STREAM_H_

#include <cstddef>
#include <cstdint>

namespace openscreen::osp {

class QuicStream {
 public:
  class Delegate {
   public:
    virtual void OnReceived(QuicStream* stream,
                            const char* data,
                            size_t data_size) = 0;
    virtual void OnClose(uint64_t stream_id) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  explicit QuicStream(Delegate* delegate) : delegate_(delegate) {}
  virtual ~QuicStream() = default;

  virtual uint64_t GetStreamId() = 0;
  virtual void Write(const uint8_t* data, size_t data_size) = 0;
  virtual void CloseWriteEnd() = 0;

 protected:
  Delegate* const delegate_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_STREAM_H_
