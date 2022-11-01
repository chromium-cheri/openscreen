// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_BYTES_VIEW_H_
#define PLATFORM_BASE_BYTES_VIEW_H_

#include <stddef.h>

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "platform/base/macros.h"

namespace openscreen {

// Contains a pointer and length to a span of continguous and unowned bytes.
// The view over the underlying data is read-only.
//
// The API is a slimmed-down version of a C++17 std::span<const uint8_t> and is
// intended to be forwards-compatible.  Support for iterators and front/back can
// be added as needed; we don't intend to add support for static extents.
//
// NOTE: Although other span implementations allow passing zero to last(), we do
// not, as the behavior is undefined.  Callers should explicitly create an empty
// BytesView instead.
//
// TODO(https://issuetracker.google.com/254502854): Replace assert() expressions
// with OSP_CHECK* macros once those are available to files in platform/.
class BytesView {
 public:
  constexpr BytesView() noexcept = default;
  constexpr BytesView(const uint8_t* data, size_t count)
      : data_(data), count_(count) {}

  constexpr BytesView(const BytesView&) noexcept = default;
  constexpr BytesView& operator=(const BytesView&) noexcept = default;
  BytesView(BytesView&&) noexcept = default;
  BytesView& operator=(BytesView&&) noexcept = default;
  ~BytesView() = default;

  constexpr const uint8_t* data() const noexcept { return data_; }

  constexpr const uint8_t& operator[](size_t idx) const {
    assert(idx < count_);
    return *(data_ + idx);
  }

  constexpr size_t size() const { return count_; }

  [[nodiscard]] constexpr bool empty() const { return count_ == 0; }

  constexpr BytesView first(size_t count) const {
    assert(count < count_);
    return BytesView(data_, count);
  }

  constexpr BytesView last(size_t count) const {
    assert(count <= count_);
    assert(count != 0);
    return BytesView(data_ + (count_ - count), count);
  }

  constexpr BytesView subspan(size_t offset, size_t count) const {
    assert(offset + count < count_);
    return BytesView(data_ + offset, count);
  }

 private:
  const uint8_t* data_{nullptr};
  size_t count_{0};
};

}  // namespace openscreen

#endif  // PLATFORM_BASE_BYTES_VIEW_H_
