// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_CHECKED_CAST_H_
#define UTIL_CHECKED_CAST_H_

#include <limits>
#include <type_traits>

#include "util/osp_logging.h"

namespace openscreen {

// Safely casts |value| to type |Dst|.  Both |value| and |Dst| must be integral,
// or floating-point types.  An underflow or overflow will result in a
// CHECK-failure.
template <typename Dst, typename Src>
constexpr Dst checked_cast(Src value) {
  static_assert(std::is_arithmetic<Src>(), "Src is not a numeric type");
  static_assert(std::is_arithmetic<Dst>(), "Dst is not a numeric type");
  OSP_CHECK_LE(value, static_cast<Src>(std::numeric_limits<Dst>::max()));
  OSP_CHECK_GE(value, static_cast<Src>(std::numeric_limits<Dst>::min()));
  return static_cast<Dst>(static_cast<Src>(value));
}

}  // namespace openscreen

#endif  // UTIL_CHECKED_CAST_H_
