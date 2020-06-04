// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_OPTIONAL_H_
#define PLATFORM_BASE_OPTIONAL_H_

#include <utility>

namespace openscreen {
// This class is meant to be a drop-in replacement for std::optional or
// Abseil optional. It helps avoid use of abseil header files, and also heap
// allocations and having to write lots of boilerplate by using either raw
// pointers or unique pointers, which typically require declaring all of the
// move/copy assignment/constructors as well as a destructor and reasonable
// defaults.
template <typename T>
class Optional {
 public:
  // Tag type used for NullOpt construction.
  struct NullOptInit {
    NullOptInit() = default;
  };

  struct NullOpt {
    // Purposefully not default-constructible.
    explicit constexpr NullOpt(NullOptInit) noexcept {}
  };

  constexpr Optional() noexcept {}
  explicit constexpr Optional(NullOpt) noexcept {}
  explicit constexpr Optional(T value)
      : has_value_(true), value_(std::move(value)) {}
  Optional(const Optional& other) = default;
  Optional(Optional&& other) = default;
  constexpr Optional& operator=(const Optional& other) = default;
  constexpr Optional& operator=(Optional&& other) noexcept = default;
  constexpr Optional& operator=(NullOpt) noexcept {
    has_value_ = false;
    return *this;
  }

  // Assignment operators for value types.
  Optional& operator=(T&& value) noexcept {
    has_value_ = true;
    value_ = std::move(value);
    return *this;
  }

  // Dereferencing operators.
  constexpr const T* operator->() const { return &value_; }
  constexpr T* operator->() { return &value_; }
  ~Optional() {}

  // Value checking.
  // NOTE: we explicitly avoid providing the bool() operator, to improve
  // readability and decrease likelihood of tricky states, e.g. if the type
  // in Optional is also implicit bool, you may not get what you expect in
  // some cases.
  // constexpr explicit operator bool() const noexcept { return has_value(); }
  constexpr bool has_value() const noexcept { return has_value_; }

  // Value getters.
  constexpr T& value() { return value_; }
  constexpr const T& value() const { return value_; }

 private:
  bool has_value_ = false;

  // Note: we don't want to call the default constructor for T when the
  // optional class is empty (it may not exist or may be non-trivial).
  // We use a union here to avoid default construction.
  union {
    NullOpt empty_;
    T value_;
  };
};

}  // namespace openscreen

#endif  // PLATFORM_BASE_OPTIONAL_H_
