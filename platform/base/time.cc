// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/time.h"

#include <ctime>
#include <ratio>

namespace openscreen {
namespace platform {

Clock::time_point Clock::now() noexcept {
  using namespace std::chrono;

  constexpr bool can_use_steady_clock =
      std::ratio_less_equal<steady_clock::period, kRequiredResolution>::value;
  constexpr bool can_use_high_resolution_clock =
      std::ratio_less_equal<high_resolution_clock::period,
                            kRequiredResolution>::value &&
      high_resolution_clock::is_steady;
  static_assert(can_use_steady_clock || can_use_high_resolution_clock,
                "no suitable default clock on this platform");

  return Clock::time_point(
      can_use_steady_clock
          ? duration_cast<Clock::duration>(
                steady_clock::now().time_since_epoch())
          : duration_cast<Clock::duration>(
                high_resolution_clock::now().time_since_epoch()));
}

std::chrono::seconds GetWallTimeSinceUnixEpoch() noexcept {
  using namespace std::chrono;

  // Note: Even though std::time_t is not necessarily "seconds since UNIX epoch"
  // before C++20, it is almost universally implemented that way on all
  // platforms. There is a unit test to confirm this behavior, so don't worry
  // about it here.
  const std::time_t since_epoch = system_clock::to_time_t(system_clock::now());
  return seconds(since_epoch);
}

}  // namespace platform
}  // namespace openscreen
