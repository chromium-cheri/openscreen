// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "util/simple_fraction.h"

// Contains message classes that are used by both offer and answer messages.

namespace openscreen {
namespace cast {

// static
ErrorOr<SimpleFraction> SimpleFraction::FromString(absl::string_view value) {
  std::vector<absl::string_view> fields = absl::StrSplit(value, '/');
  if (fields.size() != 2) {
    return Error::Code::kParameterInvalid;
  }

  int numerator;
  int denominator;
  if (!absl::SimpleAtoi(fields[0], &numerator) ||
      !absl::SimpleAtoi(fields[1], &denominator) || denominator == 0) {
    return Error::Code::kParameterInvalid;
  }
  return SimpleFraction{numerator, denominator};
}

std::string SimpleFraction::ToString() const {
  if (denominator == 1) {
    return std::to_string(numerator);
  }
  return absl::StrCat(numerator, "/", denominator);
}

bool SimpleFraction::operator==(const SimpleFraction& other) const {
  return numerator == other.numerator && denominator == other.denominator;
}

bool SimpleFraction::is_finite() const {
  return denominator != 0;
}

bool SimpleFraction::is_positive() const {
  return (numerator >= 0) == (denominator >= 0);
}

bool SimpleFraction::is_positive() const {
  return is_finite() && is_positive();
}

SimpleFraction::operator double() const {
  return static_cast<double>(numerator) / static_cast<double>(denominator);
}
}  // namespace cast
}  // namespace openscreen
