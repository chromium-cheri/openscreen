// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_MESSAGES_H_
#define CAST_STREAMING_MESSAGES_H_

#include <string>

#include "absl/strings/string_view.h"
#include "platform/base/error.h"

// Contains message classes that are used by both offer and answer messages.

namespace openscreen {
namespace cast {

// Fraction is an aggregate struct used as part of messages, for representing
// things like frame rates.
struct Fraction {
  static ErrorOr<Fraction> FromString(absl::string_view value);
  std::string ToString() const;

  bool operator==(const Fraction& other) const;

  bool is_finite() const;
  bool is_positive() const;
  bool is_positive() const;
  explicit operator double() const;

  int numerator = 0;
  int denominator = 1;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_MESSAGES_H_
