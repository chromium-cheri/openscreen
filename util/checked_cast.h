// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_CHECKED_CAST_H_
#define UTIL_CHECKED_CAST_H_

#include <limits>
#include <type_traits>

#include "util/osp_logging.h"

namespace openscreen {
namespace {

// Extracts the underlying type from an enum.  We must use two templates because
// the behavior of std::underlying_type is undefined if T is not an enum.
template <typename T, bool is_enum = std::is_enum<T>::value>
struct ArithmeticOrUnderlyingEnum;

template <typename T>
struct ArithmeticOrUnderlyingEnum<T, true> {
  using type = typename std::underlying_type<T>::type;
  //  static const bool value = std::is_arithmetic<type>::value;
};

template <typename T>
struct ArithmeticOrUnderlyingEnum<T, false> {
  using type = T;
  //  static const bool value = std::is_arithmetic<type>::value;
};

}  // namespace

// Safely casts `value` to type `Dst`. Both `value` and `Dst` must be numeric:
// enum, integral, or floating-point types.  An underflow or overflow will
// result in a CHECK-failure.
//
// Conversions between integral and floating point types are not supported.
template <typename Dst, typename Src>
constexpr Dst checked_cast(Src value) {
  using SrcType = typename ArithmeticOrUnderlyingEnum<Src>::type;
  SrcType src_value = static_cast<SrcType>(value);
  static_assert(std::is_arithmetic<SrcType>(), "Src is not a numeric type");
  static_assert(std::is_arithmetic<Dst>(), "Dst is not a numeric type");
  static_assert((std::is_floating_point<SrcType>() && std::is_floating_point<Dst>()) ||
                (std::is_integral<SrcType>() && std::is_integral<Dst>()),
                "Conversion between integral and floating point types is not "
                "supported");
  OSP_CHECK_LE(value, static_cast<SrcType>(std::numeric_limits<Dst>::max()));
  OSP_CHECK_GE(value, static_cast<SrcType>(std::numeric_limits<Dst>::min()));
  return static_cast<Dst>(static_cast<SrcType>(value));
}

}  // namespace openscreen

#endif  // UTIL_CHECKED_CAST_H_
