// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SPAN_H_
#define BASE_SPAN_H_

#include <cassert>
#include <cstdint>
#include <type_traits>

// Adapted from the Microsoft GSL implementation, which requires c++14.
namespace openscreen {

static constexpr std::ptrdiff_t dynamic_extent = -1;

namespace detail {

template <std::ptrdiff_t From, std::ptrdiff_t To>
struct is_allowed_extent_conversion
    : std::integral_constant<bool,
                             From == To || From == dynamic_extent ||
                                 To == dynamic_extent> {};

template <typename From, typename To>
struct is_allowed_extent_type_conversion
    : std::integral_constant<bool,
                             std::is_convertible<From (*)[], To (*)[]>::value> {
};

template <typename Span, bool IsConst>
class span_iterator {
 public:
  using element_type = typename Span::element_type;
  using index_type = std::ptrdiff_t;
  using reference = typename std::conditional<
      IsConst,
      typename std::add_lvalue_reference<const element_type>::type,
      typename std::add_lvalue_reference<element_type>::type>::type;
  using pointer = typename std::add_pointer<reference>::type;

  span_iterator(const Span& span, index_type index)
      : span_(span), index_(index) {}

  span_iterator& operator++() {
    assert(index_ != span_.size());
    ++index_;
    return *this;
  }

  span_iterator operator++(int) {
    assert(index_ != span_.size());
    auto ret = *this;
    ++index_;
    return ret;
  }

  span_iterator& operator--() {
    assert(index_ != 0);
    --index_;
    return *this;
  }

  span_iterator operator--(int) {
    assert(index_ != 0);
    auto ret = *this;
    --index_;
    return ret;
  }

  friend bool operator==(span_iterator lhs, span_iterator rhs) {
    return lhs.span_ == rhs.span_ && lhs.index_ == rhs.index_;
  }

  friend bool operator!=(span_iterator lhs, span_iterator rhs) {
    return !(lhs == rhs);
  }

  reference operator*() const { return span_[index_]; }

  pointer operator->() const { return &span_[index_]; }

 private:
  const Span& span_;
  index_type index_;
};

template <std::ptrdiff_t Extent>
struct extent_type {
  using index_type = std::ptrdiff_t;
  static_assert(Extent >= 0, "Fixed-size extent must have size >= 0");
  constexpr extent_type() {}
  template <index_type OtherExtent>
  extent_type(extent_type<OtherExtent> ext) {
    static_assert(Extent == OtherExtent || OtherExtent == dynamic_extent,
                  "Mismatch fixed-size extent and initializer data size");
    assert(ext.size() == Extent);
  }
  extent_type(index_type count) { assert(count == Extent); }

  constexpr index_type size() const { return Extent; }
};

template <>
struct extent_type<dynamic_extent> {
  using index_type = std::ptrdiff_t;
  template <index_type OtherExtent>
  extent_type(extent_type<OtherExtent>) : count(OtherExtent) {}
  extent_type(index_type ext) : count(ext) { assert(ext >= 0); }

  constexpr index_type size() const { return count; }

  index_type count;
};

}  // namespace detail

template <typename ElementType, std::ptrdiff_t Extent>
class span {
 public:
  static_assert(Extent >= 0 || Extent == dynamic_extent,
                "Extent must be non-negative or specify a dynamic extent");
  using element_type = ElementType;
  using index_type = std::ptrdiff_t;
  using pointer = element_type*;
  using reference = typename std::add_lvalue_reference<element_type>::type;

  using iterator = detail::span_iterator<span<ElementType, Extent>, false>;
  using const_iterator = detail::span_iterator<span<ElementType, Extent>, true>;

  span() {}
  span(pointer ptr, index_type count) : storage_(ptr, count) {}

  template <std::size_t N>
  span(ElementType (&arr)[N]) : storage_(&arr[0], detail::extent_type<N>()) {}

  span(const span& other) = default;

  template <
      typename OtherElementType,
      index_type OtherExtent,
      typename = typename std::enable_if<
          detail::is_allowed_extent_conversion<OtherExtent, Extent>::value &&
          detail::is_allowed_extent_type_conversion<OtherElementType,
                                                    ElementType>::value>::type>
  span(const span<OtherElementType, OtherExtent>& other)
      : storage_(other.data(), detail::extent_type<OtherExtent>(other.size())) {
  }

  span& operator=(const span& other) = default;

  reference operator[](index_type i) const { return storage_.data[i]; }
  pointer data() const { return storage_.data; }
  constexpr index_type size() const { return storage_.size(); }

  iterator begin() const { return iterator(*this, 0); }
  iterator end() const { return iterator(*this, size()); }
  const_iterator cbegin() const { return const_iterator(*this, 0); }
  const_iterator cend() const { return const_iterator(*this, size()); }

  friend bool operator==(const span& lhs, const span& rhs) {
    return lhs.data() == rhs.data() && lhs.size() == rhs.size();
  }

  friend bool operator!=(const span& lhs, const span& rhs) {
    return !(lhs == rhs);
  }

 private:
  template <typename ExtentType>
  struct Storage : ExtentType {
    template <std::ptrdiff_t OtherExtent>
    Storage(pointer data, detail::extent_type<OtherExtent> ext)
        : ExtentType(ext), data(data) {}
    Storage(pointer data, index_type ext) : ExtentType(ext), data(data) {
      assert(data || ext == 0);
    }

    pointer data;
  };

  Storage<detail::extent_type<Extent>> storage_;
};

}  // namespace openscreen

#endif
