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
 private:
  struct Data : Traits {
    explicit Data(typename Traits::PipeType pipe) : pipe(pipe) {}
    Data(typename Traits::PipeType pipe, const Traits& traits)
        : Traits(traits), pipe(pipe) {}
    typename Traits::PipeType pipe;
  };

 public:
  using PipeType = typename Traits::PipeType;

  ScopedPipe() : data_(Traits::kInvalidValue) {}
  explicit ScopedPipe(PipeType pipe) : data_(pipe) {}
  ScopedPipe(PipeType pipe, const Traits& traits) : data_(pipe, traits) {}
  ScopedPipe(const ScopedPipe&) = delete;
  ScopedPipe(ScopedPipe&& other) : data_(other.release(), other.get_traits()) {
  }
  ~ScopedPipe() {
    if (data_.pipe != Traits::kInvalidValue) {
      data_.Close(release());
    }
  }

  ScopedPipe& operator=(ScopedPipe other) {
    swap(other);
    return *this;
  }

  void swap(ScopedPipe& other) {
    using std::swap;
    swap(static_cast<Traits&>(other.data_), static_cast<Traits&>(data_));
    swap(other.data_.pipe, data_.pipe);
  }

  PipeType get() { return data_.pipe; }
  PipeType release() {
    PipeType pipe = data_.pipe;
    data_.pipe = Traits::kInvalidValue;
    return pipe;
  }

  bool operator==(const ScopedPipe& other) const {
    return data_.pipe == other.data_.pipe;
  }
  bool operator!=(const ScopedPipe& other) const { return !(*this == other); }

  explicit operator bool() const { return data_.pipe != Traits::kInvalidValue; }

  Traits& get_traits() { return data_; }
  const Traits& get_traits() const { return data_; }

 private:
  Data data_;
};

template <typename Traits>
void swap(ScopedPipe<Traits>& a, ScopedPipe<Traits>& b) {
  a.swap(b);
}

using ScopedFd = ScopedPipe<IntFdTraits>;

}  // namespace openscreen

#endif
