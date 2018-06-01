// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/time.h"

namespace openscreen {
namespace platform {

Milliseconds operator-(Milliseconds t1, Milliseconds t2) {
  return Milliseconds(t1.t - t2.t);
}

bool operator<(Milliseconds t1, Milliseconds t2) {
  return t1.t < t2.t;
}

bool operator<=(Milliseconds t1, Milliseconds t2) {
  return t1.t <= t2.t;
}

bool operator>(Milliseconds t1, Milliseconds t2) {
  return t1.t > t2.t;
}

bool operator>=(Milliseconds t1, Milliseconds t2) {
  return t1.t >= t2.t;
}

bool operator==(Milliseconds t1, Milliseconds t2) {
  return t1.t == t2.t;
}

bool operator!=(Milliseconds t1, Milliseconds t2) {
  return t1.t != t2.t;
}

Microseconds operator-(Microseconds t1, Microseconds t2) {
  return Microseconds(t1.t - t2.t);
}

bool operator<(Microseconds t1, Microseconds t2) {
  return t1.t < t2.t;
}

bool operator<=(Microseconds t1, Microseconds t2) {
  return t1.t <= t2.t;
}

bool operator>(Microseconds t1, Microseconds t2) {
  return t1.t > t2.t;
}

bool operator>=(Microseconds t1, Microseconds t2) {
  return t1.t >= t2.t;
}

bool operator==(Microseconds t1, Microseconds t2) {
  return t1.t == t2.t;
}

bool operator!=(Microseconds t1, Microseconds t2) {
  return t1.t != t2.t;
}

Milliseconds ToMilliseconds(Microseconds t) {
  return Milliseconds(t.t / 1000);
}

Microseconds ToMicroseconds(Milliseconds t) {
  return Microseconds(t.t * 1000);
}

}  // namespace platform
}  // namespace openscreen
