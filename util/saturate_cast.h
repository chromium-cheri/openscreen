// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_SATURATE_CAST_H_
#define UTIL_SATURATE_CAST_H_

#include <limits>
#include <type_traits>

namespace openscreen {

// Convert from one value type to another, clamping to the min/max of the new
// value type's range if necessary.
template <typename To, typename From>
constexpr To saturate_cast(From from) {
  static_assert(std::numeric_limits<From>::is_integer &&
                    std::numeric_limits<To>::is_integer,
                "Non-integral saturate_cast is not implemented.");

  // Because of the way C++ signed versus unsigned comparison works (i.e., the
  // type promotion strategy employed), extra care must be taken to range-check
  // the input value. For example, if the current architecture is 32-bits, then
  // any int32_t compared with a uint32_t will NOT promote to a int64_t↔int64_t
  // comparison. Instead, it will become a uint32_t↔uint32_t comparison (!),
  // which will sometimes produce invalid results.

  constexpr auto to_max = std::numeric_limits<To>::max();
  constexpr auto from_max = std::numeric_limits<From>::max();
  constexpr auto to_min = std::numeric_limits<To>::min();
  constexpr auto from_min = std::numeric_limits<From>::min();

  if (std::numeric_limits<From>::is_signed ==
      std::numeric_limits<To>::is_signed) {
    // Case 1: "From" and "To" are either both signed, or are both unsigned. In
    // this case, the smaller of the two types will be promoted to match the
    // larger's size, and a valid comparison will be made.
    if ((from_min <= to_min) && (from <= static_cast<From>(to_min))) {
      return to_min;
    }

    if ((from_max >= to_max) && (from >= static_cast<From>(to_max))) {
      return to_max;
    }
  } else if (std::numeric_limits<From>::is_signed) {
    // Case 2: "From" is signed, but "To" is unsigned.
    if (from <= From{0}) {
      return To{0};
    }

    if ((from_max >= to_max) && (from >= static_cast<From>(to_max))) {
      return to_max;
    }
  } else {
    // Case 3: "From" is unsigned, but "To" is signed.
    if ((from_max >= to_max) && (from >= static_cast<From>(to_max))) {
      return to_max;
    }
    // Note: "From" can never be less than "To's" minimum value.
  }

  // No clamping is needed: |from| is within the representable value range.
  return static_cast<To>(from);
}

}  // namespace openscreen

#endif  // UTIL_SATURATE_CAST_H_
