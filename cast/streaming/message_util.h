// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_MESSAGE_UTIL_H_
#define CAST_STREAMING_MESSAGE_UTIL_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "json/value.h"
#include "platform/base/error.h"

// This file contains helper methods that are used by both answer and offer
// messages, but should not be publicly exposed/consumed.
namespace openscreen {
namespace cast {

inline Error CreateParseError(const std::string& type) {
  return Error(Error::Code::kJsonParseError,
               "Failed to parse field, expected " + type);
}

inline Error CreateParameterError(const std::string& type) {
  return Error(Error::Code::kParameterInvalid, "Invalid parameter: " + type);
}

inline ErrorOr<bool> ParseBool(const Json::Value& value) {
  if (!value.isBool()) {
    return CreateParseError("bool");
  }
  return value.asBool();
}

inline ErrorOr<double> ParseDouble(const Json::Value& value) {
  if (!value.isDouble()) {
    return CreateParseError("double");
  }
  return value.asDouble();
}

inline ErrorOr<int> ParseInt(const Json::Value& value) {
  if (!value.isInt()) {
    return CreateParseError("integer");
  }
  return value.asInt();
}

inline ErrorOr<uint32_t> ParseUint(const Json::Value& value) {
  if (!value.isUInt()) {
    return CreateParseError("unsigned integer");
  }
  return value.asUInt();
}

inline ErrorOr<std::string> ParseString(const Json::Value& value) {
  if (!value.isString()) {
    return CreateParseError("string");
  }
  return value.asString();
}

// We want to be more robust when we parse fractions then just
// allowing strings, this will parse numeral values such as
// value: 50 as well as value: "50" and value: "100/2".
inline ErrorOr<SimpleFraction> ParsePositiveSimpleFraction(
    const Json::Value& value) {
  if (value.isInt()) {
    int parsed = value.asInt();
    if (parsed < 0) {
      return Error::Code::kParameterInvalid;
    }
    return SimpleFraction{parsed, 1};
  }

  if (value.isString()) {
    auto fraction_or_error = SimpleFraction::FromString(value.asString());
    if (!fraction_or_error) {
      return std::move(fraction_or_error.error());
    }

    if (!fraction_or_error.value().is_positive() ||
        !fraction_or_error.value().is_defined()) {
      return Error::Code::kParameterInvalid;
    }
    return fraction_or_error.value();
  }
  return Error::Code::kParameterInvalid;
}

template <typename T>
using Parser = std::function<ErrorOr<T>(const Json::Value&)>;

template <typename T>
ErrorOr<std::vector<T>> ParseArray(const Json::Value& value, Parser<T> parser) {
  if (!value.isArray() || value.empty()) {
    return Error::Code::kParameterInvalid;
  }

  std::vector<T> out;
  out.reserve(value.size());
  for (Json::ArrayIndex i = 0; i < value.size(); ++i) {
    auto v = parser(value[i]);
    if (!v) {
      return v.error();
    }
    out.emplace_back(v.value());
  }

  return out;
}

inline ErrorOr<std::vector<int>> ParseIntArray(const Json::Value& value) {
  return ParseArray<int>(value, ParseInt);
}

inline ErrorOr<std::vector<uint32_t>> ParseUintArray(const Json::Value& value) {
  return ParseArray<uint32_t>(value, ParseUint);
}

inline ErrorOr<std::vector<std::string>> ParseStringArray(
    const Json::Value& value) {
  return ParseArray<std::string>(value, ParseString);
}

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_MESSAGE_UTIL_H_
