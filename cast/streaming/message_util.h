// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_MESSAGE_UTIL_H_
#define CAST_STREAMING_MESSAGE_UTIL_H_

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "json/value.h"
#include "platform/base/error.h"
namespace openscreen {
namespace cast {

inline Error CreateParseError(const std::string& type) {
  return Error(Error::Code::kJsonParseError, "Failed to parse " + type);
}

inline Error CreateParameterError(const std::string& type) {
  return Error(Error::Code::kParameterInvalid, "Invalid parameter: " + type);
}

inline ErrorOr<bool> ParseBool(const Json::Value& parent,
                               const std::string& field) {
  const Json::Value& value = parent[field];
  if (!value.isBool()) {
    return CreateParseError("bool field " + field);
  }
  return value.asBool();
}

inline ErrorOr<int> ParseInt(const Json::Value& parent,
                             const std::string& field) {
  const Json::Value& value = parent[field];
  if (!value.isInt()) {
    return CreateParseError("integer field: " + field);
  }
  return value.asInt();
}

inline ErrorOr<uint32_t> ParseUint(const Json::Value& parent,
                                   const std::string& field) {
  const Json::Value& value = parent[field];
  if (!value.isUInt()) {
    return CreateParseError("unsigned integer field: " + field);
  }
  return value.asUInt();
}

inline ErrorOr<std::string> ParseString(const Json::Value& parent,
                                        const std::string& field) {
  const Json::Value& value = parent[field];
  if (!value.isString()) {
    return CreateParseError("string field: " + field);
  }
  return value.asString();
}

// TODO(jophba): refactor to be on ErrorOr itself.
// Use this template for parsing only when there is a reasonable default
// for the type you are using, e.g. int or std::string.
template <typename T>
T ValueOrDefault(const ErrorOr<T>& value, T fallback = T{}) {
  if (value.is_value()) {
    return value.value();
  }
  return fallback;
}

struct Fraction {
  static inline ErrorOr<Fraction> FromString(absl::string_view value) {
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
    return Fraction{numerator, denominator};
  }

  inline std::string ToString() const {
    return absl::StrCat(numerator, "/", denominator);
  }

  inline bool is_finite() const { return denominator != 0; }

  inline bool is_positive() const { return numerator > 0 == denominator > 0; }

  int numerator;
  int denominator;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_MESSAGE_UTIL_H_
