// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TIME_H_
#define PLATFORM_API_TIME_H_

#include <cstdint>

namespace openscreen {
namespace platform {

struct Milliseconds {
  constexpr explicit Milliseconds(int64_t t) : t(t) {}

  int64_t t;
};

Milliseconds operator-(Milliseconds t1, Milliseconds t2);
bool operator<(Milliseconds t1, Milliseconds t2);
bool operator<=(Milliseconds t1, Milliseconds t2);
bool operator>(Milliseconds t1, Milliseconds t2);
bool operator>=(Milliseconds t1, Milliseconds t2);
bool operator==(Milliseconds t1, Milliseconds t2);
bool operator!=(Milliseconds t1, Milliseconds t2);

struct Microseconds {
  constexpr explicit Microseconds(int64_t t) : t(t) {}

  int64_t t;
};

Microseconds operator-(Microseconds t1, Microseconds t2);
bool operator<(Microseconds t1, Microseconds t2);
bool operator<=(Microseconds t1, Microseconds t2);
bool operator>(Microseconds t1, Microseconds t2);
bool operator>=(Microseconds t1, Microseconds t2);
bool operator==(Microseconds t1, Microseconds t2);
bool operator!=(Microseconds t1, Microseconds t2);

Milliseconds ToMilliseconds(Microseconds t);
Microseconds ToMicroseconds(Milliseconds t);

Microseconds GetMonotonicTimeNow();
Microseconds GetUTCNow();

}  // namespace platform
}  // namespace openscreen

#endif
