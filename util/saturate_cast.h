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
                "Float (or non-integer) saturation not currently implemented.");

  // Four cases: Both "From" and "To" are signed ints, both are unsigned ints,
  // or one is signed and the other unsigned. The compiler will optimize the
  // following to just the subset of checks necessary for whichever combination
  // of the two types is being used.
  if (std::numeric_limits<From>::is_signed ==
      std::numeric_limits<To>::is_signed) {
    if (from >= std::numeric_limits<To>::max()) {
      return std::numeric_limits<To>::max();
    }
    if (from <= std::numeric_limits<To>::min()) {
      return std::numeric_limits<To>::min();
    }
  } else if (std::numeric_limits<From>::is_signed /* and To is unsigned */) {
    if (from >= std::numeric_limits<To>::max()) {
      return std::numeric_limits<To>::max();
    }
    if (from <= 0) {
      return 0;
    }
  } else /* if (From is unsigned and To is signed) */ {
    if (from >= std::numeric_limits<To>::max()) {
      return std::numeric_limits<To>::max();
    }
  }

  // No clamping is needed: |from| is within the representable value range.
  return static_cast<To>(from);
}

}  // namespace openscreen

#endif  // UTIL_SATURATE_CAST_H_
