// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/simple_fraction.h"

#include <cmath>
#include <limits>
#include <sstream>
#include <vector>

namespace openscreen {
namespace {

constexpr char kDelimiter[] = "/";

// If strtol fails, it returns zero, which may be valid in some cases. Thus,
// we also have to check errno.
bool IsStrtolError(int result) {
  return result == 0 && errno != 0;
}

}  // namespace

// static
ErrorOr<SimpleFraction> SimpleFraction::FromString(const std::string& value) {
  errno = 0;

  std::size_t delimiter_pos = value.find(kDelimiter);
  // First case: simple number, not a fraction.
  if (delimiter_pos == std::string::npos) {
    const int numerator = strtol(value.data(), nullptr, 10);
    if (IsStrtolError(numerator)) {
      return Error::Code::kParameterInvalid;
    }
    return SimpleFraction{numerator, 1};
  }

  // Second case: proper fraction.
  std::string first_field = value.substr(0, delimiter_pos);
  std::string second_field = value.substr(delimiter_pos + 1);
  int numerator = strtol(first_field.data(), nullptr, 10);
  int denominator = strtol(second_field.data(), nullptr, 10);
  if (IsStrtolError(numerator) || IsStrtolError(denominator)) {
    return Error::Code::kParameterInvalid;
  }
  return SimpleFraction{numerator, denominator};
}

std::string SimpleFraction::ToString() const {
  if (denominator == 1) {
    return std::to_string(numerator);
  }
  std::ostringstream ss;
  ss << numerator << kDelimiter << denominator;
  return ss.str();
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

bool SimpleFraction::is_positive() const {
  return is_defined() && (numerator >= 0) && (denominator > 0);
}

SimpleFraction::operator double() const {
  if (denominator == 0) {
    return nan("");
  }
  return static_cast<double>(numerator) / static_cast<double>(denominator);
}

}  // namespace openscreen
