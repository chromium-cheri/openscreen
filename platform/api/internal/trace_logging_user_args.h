// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_INTERNAL_TRACE_LOGGING_USER_ARGS_H_
#define PLATFORM_API_INTERNAL_TRACE_LOGGING_USER_ARGS_H_

#include <functional>
#include <iostream>
#include <memory>
#include <sstream>

#include "absl/types/optional.h"
#include "platform/api/trace_logging_types.h"
#include "platform/base/macros.h"

namespace openscreen {
namespace platform {
namespace internal {
namespace {
constexpr const char* kNullptrString = "nullptr";
}  // namespace

// Struct to store name and value for a given user-provided argument.
class TraceLoggingArgumentBase {
 public:
  virtual ~TraceLoggingArgumentBase() = default;

  const char* name() const { return name_; }
  virtual UserArgumentValue value() const = 0;

 protected:
  TraceLoggingArgumentBase(const char* name) : name_(name) {}

 private:
  const char* name_;
};

// Generic case which gets the argument value with a stringstream.
// NOTE: This will not work for non-copyable types.
template <typename T>
class TraceLoggingArgument : public TraceLoggingArgumentBase {
 public:
  TraceLoggingArgument(const char* name, T value)
      : TraceLoggingArgumentBase(name), value_(value) {}

  ~TraceLoggingArgument() override = default;

  UserArgumentValue value() const override {
    if (!result_.has_value()) {
      std::stringstream outstream;
      outstream << value_;
      result_.emplace(outstream.str().c_str());
    }
    return result_.value();
  }

 private:
  const T value_;
  mutable absl::optional<UserArgumentValue> result_;
};

// Special case for nullptr
template <>
class TraceLoggingArgument<std::nullptr_t> : public TraceLoggingArgumentBase {
 public:
  TraceLoggingArgument(const char* name, const std::nullptr_t* value)
      : TraceLoggingArgumentBase(name) {}
  TraceLoggingArgument(const char* name, const std::nullptr_t& value)
      : TraceLoggingArgumentBase(name) {}
  ~TraceLoggingArgument() override = default;

  UserArgumentValue value() const override { return value_; }

 private:
  const UserArgumentValue value_ = UserArgumentValue(kNullptrString);
};

// Special cases for integer types
template <>
class TraceLoggingArgument<int64_t> : public TraceLoggingArgumentBase {
 public:
  TraceLoggingArgument(const char* name, const int64_t value)
      : TraceLoggingArgumentBase(name), value_(value) {}
  ~TraceLoggingArgument() override = default;

  UserArgumentValue value() const override { return value_; }

 private:
  const UserArgumentValue value_;
};

template <>
class TraceLoggingArgument<double> : public TraceLoggingArgumentBase {
 public:
  TraceLoggingArgument(const char* name, const double value)
      : TraceLoggingArgumentBase(name), value_(value) {}
  ~TraceLoggingArgument() override = default;

  UserArgumentValue value() const override { return value_; }

 private:
  const UserArgumentValue value_;
};

}  // namespace internal
}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_INTERNAL_TRACE_LOGGING_USER_ARGS_H_
