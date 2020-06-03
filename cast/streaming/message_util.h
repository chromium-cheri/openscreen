// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_MESSAGE_UTIL_H_
#define CAST_STREAMING_MESSAGE_UTIL_H_

#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "json/value.h"
#include "platform/base/error.h"

// This file contains helper methods that are used by both answer and offer
// messages, but should not be publicly exposed/consumed.
namespace openscreen {
namespace cast {

// TODO(jophba): remove these methods after refactoring offer messaging.
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

// TODO(jophba): offer messaging should use these methods instead.
inline Error CreateMessageError(const std::string& type) {
  return Error(Error::Code::kParameterInvalid, "Invalid message: " + type);
}

inline bool ParseBool(const Json::Value& value, bool* out) {
  if (!value.isBool()) {
    return false;
  }
  *out = value.asBool();
  return true;
}

inline bool ParseAndValidateDouble(const Json::Value& value, double* out) {
  if (!value.isDouble()) {
    return false;
  }
  const double d = value.asDouble();
  if (d <= 0) {
    return false;
  }
  *out = d;
  return true;
}

inline bool ParseAndValidateInt(const Json::Value& value, int* out) {
  if (!value.isInt()) {
    return false;
  }
  int i = value.asInt();
  if (i < 0) {
    return false;
  }
  *out = i;
  return true;
}

inline bool ParseAndValidateUint(const Json::Value& value, uint32_t* out) {
  if (!value.isUInt()) {
    return false;
  }
  *out = value.asUInt();
  return true;
}

inline bool ParseAndValidateString(const Json::Value& value, std::string* out) {
  if (!value.isString()) {
    return false;
  }
  *out = value.asString();
  return true;
}

// We want to be more robust when we parse fractions then just
// allowing strings, this will parse numeral values such as
// value: 50 as well as value: "50" and value: "100/2".
inline bool ParseAndValidateSimpleFraction(const Json::Value& value,
                                           SimpleFraction* out) {
  if (value.isInt()) {
    int parsed = value.asInt();
    if (parsed < 0) {
      return false;
    }
    *out = SimpleFraction{parsed, 1};
    return true;
  }

  if (value.isString()) {
    auto fraction_or_error = SimpleFraction::FromString(value.asString());
    if (!fraction_or_error) {
      return false;
    }

    if (!fraction_or_error.value().is_positive() ||
        !fraction_or_error.value().is_defined()) {
      return false;
    }
    *out = std::move(fraction_or_error.value());
    return true;
  }
  return false;
}

inline bool ParseAndValidateMilliseconds(const Json::Value& value,
                                         std::chrono::milliseconds* out) {
  int out_ms;
  if (!ParseAndValidateInt(value, &out_ms) || out_ms < 0) {
    return false;
  }
  *out = std::chrono::milliseconds(out_ms);
  return true;
}

template <typename T>
using Parser = std::function<bool(const Json::Value&, T*)>;

// NOTE: array parsing methods reset the output vector to an empty vector in
// any error case. This is especially useful for optional arrays.
template <typename T>
bool ParseAndValidateArray(const Json::Value& value,
                           Parser<T> parser,
                           std::vector<T>* out) {
  out->clear();
  if (!value.isArray() || value.empty()) {
    return false;
  }

  out->reserve(value.size());
  for (Json::ArrayIndex i = 0; i < value.size(); ++i) {
    T v;
    if (!parser(value[i], &v)) {
      out->clear();
      return false;
    }
    out->push_back(v);
  }

  return true;
}

inline bool ParseAndValidateIntArray(const Json::Value& value,
                                     std::vector<int>* out) {
  return ParseAndValidateArray<int>(value, ParseAndValidateInt, out);
}

inline bool ParseAndValidateUintArray(const Json::Value& value,
                                      std::vector<uint32_t>* out) {
  return ParseAndValidateArray<uint32_t>(value, ParseAndValidateUint, out);
}

inline bool ParseAndValidateStringArray(const Json::Value& value,
                                        std::vector<std::string>* out) {
  return ParseAndValidateArray<std::string>(value, ParseAndValidateString, out);
}

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_MESSAGE_UTIL_H_
