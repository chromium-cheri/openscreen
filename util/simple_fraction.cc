// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/simple_fraction.h"

#include <limits>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "util/logging.h"

namespace openscreen {

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

bool SimpleFraction::operator!=(const SimpleFraction& other) const {
  return !(*this == other);
}

bool SimpleFraction::is_defined() const {
  return denominator != 0;
}

bool SimpleFraction::is_finite() const {
  return numerator > std::numeric_limits<int>::min() &&
         numerator < std::numeric_limits<int>::max() && is_defined();
}

bool SimpleFraction::is_positive() const {
  return is_defined() && (numerator >= 0) && (denominator > 0);
}

SimpleFraction::operator double() const {
  OSP_DCHECK(denominator != 0);
  return static_cast<double>(numerator) / static_cast<double>(denominator);
}

}  // namespace openscreen
