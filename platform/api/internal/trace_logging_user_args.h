// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_INTERNAL_TRACE_LOGGING_USER_ARGS_H_
#define PLATFORM_API_INTERNAL_TRACE_LOGGING_USER_ARGS_H_

#include <functional>
#include <iostream>
#include <memory>
#include <sstream>

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
  TraceLoggingArgumentBase(TraceLoggingArgumentBase&& other) = default;
  virtual ~TraceLoggingArgumentBase() = default;
  TraceLoggingArgumentBase& operator=(TraceLoggingArgumentBase&& other) =
      default;

  const char* name() const { return name_; }
  virtual const char* value() const = 0;

  virtual bool is_set() { return true; }

 protected:
  TraceLoggingArgumentBase(const char* name) : name_(name) {}

 private:
  const char* name_;

  OSP_DISALLOW_COPY_AND_ASSIGN(TraceLoggingArgumentBase);
};

// Generic case which gets the argument value with a stringstream.
// NOTE: This will not work for non-copyable types.
template <typename T>
class TraceLoggingArgument : public TraceLoggingArgumentBase {
 public:
  TraceLoggingArgument(const char* name, T value)
      : TraceLoggingArgumentBase(name), value_(value) {}

  TraceLoggingArgument(TraceLoggingArgument&& other) = default;
  ~TraceLoggingArgument() override = default;
  TraceLoggingArgument& operator=(TraceLoggingArgument&& other) = default;

  const char* value() const override {
    if (!executed) {
      std::stringstream outstream;
      outstream << value_;
      string_value_ = outstream.str();
      executed = true;
    }
    return string_value_.c_str();
  }

 private:
  const T value_;
  mutable std::string string_value_;
  mutable bool executed = false;

  OSP_DISALLOW_COPY_AND_ASSIGN(TraceLoggingArgument);
};

// Specialization for pointers
template <typename T>
class TraceLoggingArgument<T*> : public TraceLoggingArgumentBase {
 public:
  TraceLoggingArgument(const char* name, T* value)
      : TraceLoggingArgumentBase(name) {}
  TraceLoggingArgument(TraceLoggingArgument&& other) = default;
  ~TraceLoggingArgument() override = default;
  TraceLoggingArgument& operator=(TraceLoggingArgument&& other) = default;

  const char* value() const override {
    if (!executed) {
      std::stringstream outstream;
      if (value_ == nullptr) {
        outstream << kNullptrString;
      } else {
        outstream << static_cast<const void*>(value_);
      }
      string_value_ = outstream.str();
      executed = true;
    }
    return string_value_.c_str();
  }

 private:
  const T* value_;
  mutable std::string string_value_;
  mutable bool executed = false;

  OSP_DISALLOW_COPY_AND_ASSIGN(TraceLoggingArgument);
};

// Special case for when a const char* is already provided.
template <>
class TraceLoggingArgument<const char*> : public TraceLoggingArgumentBase {
 public:
  TraceLoggingArgument(const char* name, const char* value)
      : TraceLoggingArgumentBase(name), value_(value) {}
  TraceLoggingArgument(TraceLoggingArgument&& other) = default;
  ~TraceLoggingArgument() override = default;
  TraceLoggingArgument& operator=(TraceLoggingArgument&& other) = default;

  const char* value() const override {
    return value_ == nullptr ? kNullptrString : value_;
  }

 private:
  const char* value_;

  OSP_DISALLOW_COPY_AND_ASSIGN(TraceLoggingArgument);
};

// Special case for nullptr
template <>
class TraceLoggingArgument<std::nullptr_t> : public TraceLoggingArgumentBase {
 public:
  TraceLoggingArgument(const char* name, const std::nullptr_t* value)
      : TraceLoggingArgumentBase(name) {}
  TraceLoggingArgument(const char* name, const std::nullptr_t& value)
      : TraceLoggingArgumentBase(name) {}
  TraceLoggingArgument(TraceLoggingArgument&& other) = default;
  ~TraceLoggingArgument() override = default;
  TraceLoggingArgument& operator=(TraceLoggingArgument&& other) = default;

  const char* value() const override { return kNullptrString; }

 private:
  OSP_DISALLOW_COPY_AND_ASSIGN(TraceLoggingArgument);
};

template <>
class TraceLoggingArgument<void> : public TraceLoggingArgumentBase {
 public:
  TraceLoggingArgument() : TraceLoggingArgumentBase("") {}
  TraceLoggingArgument(TraceLoggingArgument&& other) = default;
  ~TraceLoggingArgument() override = default;
  TraceLoggingArgument& operator=(TraceLoggingArgument&& other) = default;

  const char* value() const override { return ""; }

  OSP_DISALLOW_COPY_AND_ASSIGN(TraceLoggingArgument);
};

// Factory to create TraceLoggingArguments, as well as abstract away the
// template parameter since it can be inferred from the function call.
class TraceLoggingArgumentFactory {
 public:
  template <typename T>
  static TraceLoggingArgument<T> Create(const char* name, T value) {
    return TraceLoggingArgument<T>(name, value);
  }
};

}  // namespace internal
}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_INTERNAL_TRACE_LOGGING_USER_ARGS_H_
