// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/trivial_clock_traits.h"

namespace openscreen {

std::ostream& operator<<(std::ostream& os,
                         const TrivialClockTraits::duration& d) {
  constexpr char kUnits[] = "\u03BCs";  // Greek Mu + "s"
  return os << d.count() << kUnits;
}

std::ostream& operator<<(std::ostream& os,
                         const TrivialClockTraits::time_point& tp) {
  constexpr char kUnits[] = "\u03BCs-ticks";  // Greek Mu + "s-ticks"
  return os << tp.time_since_epoch().count() << kUnits;
}

}  // namespace openscreen

namespace std {
namespace chrono {

ostream& operator<<(ostream& out, const hours& h) {
  return (out << h.count() << " hours");
}

ostream& operator<<(ostream& out, const minutes& m) {
  return (out << m.count() << " minutes");
}

ostream& operator<<(ostream& out, const seconds& s) {
  return (out << s.count() << " seconds");
}

ostream& operator<<(ostream& out, const milliseconds& ms) {
  return (out << ms.count() << " ms");
}

ostream& operator<<(ostream& out, const microseconds& µs) {
  return (out << µs.count() << " µs");
}

}  // namespace chrono
}  // namespace std
