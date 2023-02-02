// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_BYTE_BUFFER_H_
#define PLATFORM_BASE_BYTE_BUFFER_H_

#include <stddef.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace openscreen {

// Contains a pointer and length to a span of continguous and unowned bytes.
// Unlike ByteView, the underlying data is mutable.
//
// The API is a slimmed-down version of a C++20 std::span<uint8_t> and is
// intended to be forwards-compatible.
//
// NOTES:
class ByteBuffer {
 public:
  constexpr ByteBuffer() noexcept = default;
  constexpr ByteBuffer(uint8_t* data, size_t count)
      : data_(data), count_(count) {}
  explicit ByteBuffer(std::vector<uint8_t>& v)
      : data_(v.data()), count_(v.size()) {}

  constexpr ByteBuffer(const ByteBuffer&) noexcept = default;
  constexpr ByteBuffer& operator=(const ByteBuffer&) noexcept = default;
  ByteBuffer(ByteBuffer&&) noexcept = default;
  ByteBuffer& operator=(ByteBuffer&&) noexcept = default;
  ~ByteBuffer() = default;

  constexpr uint8_t* data() const noexcept { return data_; }

  constexpr uint8_t& operator[](size_t idx) const {
    assert(idx < count_);
    return *(data_ + idx);
  }

  constexpr size_t size() const { return count_; }

  [[nodiscard]] constexpr bool empty() const { return count_ == 0; }

  constexpr ByteBuffer first(size_t count) const {
    assert(count <= count_);
    return ByteBuffer(data_, count);
  }

  constexpr ByteBuffer last(size_t count) const {
    assert(count <= count_);
    assert(count != 0);
    return ByteBuffer(data_ + (count_ - count), count);
  }

  constexpr uint8_t* begin() const noexcept { return data_; }
  constexpr uint8_t* end() const noexcept { return data_ + count_; }
  constexpr const uint8_t* cbegin() const noexcept { return data_; }
  constexpr const uint8_t* cend() const noexcept { return data_ + count_; }

  void remove_prefix(size_t count) noexcept {
    assert(count_ >= count);
    data_ += count;
    count_ -= count;
  }

  void remove_suffix(size_t count) noexcept {
    assert(count_ >= count);
    count_ -= count;
  }

  constexpr ByteBuffer subspan(size_t offset, size_t count) const {
    assert(offset + count < count_);
    return ByteBuffer(data_ + offset, count);
  }

 private:
  uint8_t* data_{nullptr};
  size_t count_{0};
};

}  // namespace openscreen

#endif  // PLATFORM_BASE_BYTE_BUFFER_H_
