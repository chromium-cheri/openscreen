// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SCOPED_PIPE_
#define BASE_SCOPED_PIPE_

#include <unistd.h>

#include <utility>

namespace openscreen {

struct IntFdTraits {
  using PipeType = int;
  static constexpr int kInvalidValue = -1;

  static void Close(PipeType pipe) { close(pipe); }
};

template <typename Traits>
class ScopedPipe {
 public:
  using PipeType = typename Traits::PipeType;

  ScopedPipe() : pipe_(Traits::kInvalidValue) {}
  explicit ScopedPipe(PipeType pipe) : pipe_(pipe) {}
  ScopedPipe(const ScopedPipe&) = delete;
  ScopedPipe(ScopedPipe&& other) : pipe_(other.release()) {}
  ~ScopedPipe() {
    if (pipe_ != Traits::kInvalidValue)
      Traits::Close(release());
  }

  ScopedPipe& operator=(ScopedPipe&& other) {
    if (pipe_ != Traits::kInvalidValue)
      Traits::Close(release());
    pipe_ = other.release();
    return *this;
  }

  PipeType get() { return pipe_; }
  PipeType release() {
    PipeType pipe = pipe_;
    pipe_ = Traits::kInvalidValue;
    return pipe;
  }

  bool operator==(const ScopedPipe& other) const {
    return pipe_ == other.pipe_;
  }
  bool operator!=(const ScopedPipe& other) const { return !(*this == other); }

  explicit operator bool() const { return pipe_ != Traits::kInvalidValue; }

 private:
  PipeType pipe_;
};

using ScopedFd = ScopedPipe<IntFdTraits>;

}  // namespace openscreen

#endif
