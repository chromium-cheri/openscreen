// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRING_SPAN_H_
#define BASE_STRING_SPAN_H_

#include <string>

#include "base/span.h"

// Adapted from the Microsoft GSL implementation, which requires c++14.
namespace openscreen {

template <typename CharT, std::ptrdiff_t Extent>
class basic_string_span {
 public:
  using index_type = std::ptrdiff_t;
  using pointer = CharT*;
  using span_type = span<CharT, Extent>;
  using reference = typename span_type::reference;

  using iterator = typename span_type::iterator;
  using const_iterator = typename span_type::const_iterator;

  basic_string_span() {}
  basic_string_span(pointer ptr, index_type count) : span_(ptr, count) {}

  template <std::size_t N>
  basic_string_span(CharT (&arr)[N]) : span_(remove_z(arr)) {}
  basic_string_span(std::string& s)
      : span_(&s[0], static_cast<index_type>(s.size())) {}
  basic_string_span(const std::string& s)
      : span_(&s[0], static_cast<index_type>(s.size())) {}

  reference operator[](index_type i) const { return span_[i]; }
  pointer data() const { return span_.data(); }
  constexpr index_type size() const { return span_.size(); }

  iterator begin() const { return iterator(span_, 0); }
  iterator end() const { return iterator(span_, size()); }
  const_iterator cbegin() const { return const_iterator(span_, 0); }
  const_iterator cend() const { return const_iterator(span_, size()); }

  friend bool operator==(const basic_string_span& lhs,
                         const basic_string_span& rhs) {
    return lhs.span_ == rhs.span_ && lhs.size() == rhs.size();
  }

  friend bool operator!=(const basic_string_span& lhs,
                         const basic_string_span& rhs) {
    return !(lhs == rhs);
  }

 private:
  template <std::size_t N>
  static span_type remove_z(CharT (&arr)[N]) {
    if (arr[N - 1] == 0) {
      return span_type(&arr[0], N - 1);
    } else {
      return span_type(arr);
    }
  }

  span<CharT, Extent> span_;
};

template <std::ptrdiff_t Extent = dynamic_extent>
using string_span = basic_string_span<char, Extent>;
template <std::ptrdiff_t Extent = dynamic_extent>
using cstring_span = basic_string_span<const char, Extent>;

}  // namespace openscreen

#endif
