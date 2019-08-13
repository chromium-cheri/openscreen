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
class TraceLoggingArgument {
 public:
  TraceLoggingArgument(const char* name) : name_(name) {}
  TraceLoggingArgument(TraceLoggingArgument&& other) = default;
  virtual ~TraceLoggingArgument() = default;
  TraceLoggingArgument& operator=(TraceLoggingArgument&& other) = default;

  const char* name() const { return name_; }
  virtual const char* value() const = 0;

  virtual bool is_set() { return true; }

 private:
  const char* name_;

  OSP_DISALLOW_COPY_AND_ASSIGN(TraceLoggingArgument);
};

// Generic case which gets the argument value with a stringstream.
// NOTE: This will not work for non-copyable types.
template <typename T>
class TraceLoggingArgInternal : public TraceLoggingArgument {
 public:
  TraceLoggingArgInternal(const char* name, T value)
      : TraceLoggingArgument(name), value_(value) {}

  TraceLoggingArgInternal(TraceLoggingArgInternal&& other) = default;
  ~TraceLoggingArgInternal() override = default;
  TraceLoggingArgInternal& operator=(TraceLoggingArgInternal&& other) = default;

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

  OSP_DISALLOW_COPY_AND_ASSIGN(TraceLoggingArgInternal);
};

// Special case for when a function is provided. This allows for lazy
// evaluation the argument at the end of the tracing block.
template <typename FuncType>
class TraceLoggingArgInternal<std::function<FuncType()>>
    : public TraceLoggingArgument {
 public:
  TraceLoggingArgInternal(const char* name,
                          std::function<FuncType()> value_factory)
      : TraceLoggingArgument(name), value_factory_(value_factory) {}
  TraceLoggingArgInternal(TraceLoggingArgInternal&& other) = default;
  ~TraceLoggingArgInternal() override = default;
  TraceLoggingArgInternal& operator=(TraceLoggingArgInternal&& other) = default;

  const char* value() const override {
    if (!executed) {
      std::stringstream outstream;
      outstream << value_factory_();
      string_value_ = outstream.str();
      executed = true;
    }
    return string_value_.c_str();
  }

 private:
  const std::function<FuncType()> value_factory_;
  mutable std::string string_value_;
  mutable bool executed = false;

  OSP_DISALLOW_COPY_AND_ASSIGN(TraceLoggingArgInternal);
};

// Specialization for pointers
template <typename T>
class TraceLoggingArgInternal<T*> : public TraceLoggingArgument {
 public:
  TraceLoggingArgInternal(const char* name, T* value)
      : TraceLoggingArgument(name) {}
  TraceLoggingArgInternal(TraceLoggingArgInternal&& other) = default;
  ~TraceLoggingArgInternal() override = default;
  TraceLoggingArgInternal& operator=(TraceLoggingArgInternal&& other) = default;

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

  OSP_DISALLOW_COPY_AND_ASSIGN(TraceLoggingArgInternal);
};

// Special case for when a const char* is already provided.
template <>
class TraceLoggingArgInternal<const char*> : public TraceLoggingArgument {
 public:
  TraceLoggingArgInternal(const char* name, const char* value)
      : TraceLoggingArgument(name), value_(value) {}
  TraceLoggingArgInternal(TraceLoggingArgInternal&& other) = default;
  ~TraceLoggingArgInternal() override = default;
  TraceLoggingArgInternal& operator=(TraceLoggingArgInternal&& other) = default;

  const char* value() const override {
    return value_ == nullptr ? kNullptrString : value_;
  }

 private:
  const char* value_;

  OSP_DISALLOW_COPY_AND_ASSIGN(TraceLoggingArgInternal);
};

// Special case for nullptr
template <>
class TraceLoggingArgInternal<std::nullptr_t> : public TraceLoggingArgument {
 public:
  TraceLoggingArgInternal(const char* name, const std::nullptr_t* value)
      : TraceLoggingArgument(name) {}
  TraceLoggingArgInternal(const char* name, const std::nullptr_t& value)
      : TraceLoggingArgument(name) {}
  TraceLoggingArgInternal(TraceLoggingArgInternal&& other) = default;
  ~TraceLoggingArgInternal() override = default;
  TraceLoggingArgInternal& operator=(TraceLoggingArgInternal&& other) = default;

  const char* value() const override { return kNullptrString; }

 private:
  OSP_DISALLOW_COPY_AND_ASSIGN(TraceLoggingArgInternal);
};

template <>
class TraceLoggingArgInternal<void> : public TraceLoggingArgument {
 public:
  TraceLoggingArgInternal() : TraceLoggingArgument("") {}
  TraceLoggingArgInternal(TraceLoggingArgInternal&& other) = default;
  ~TraceLoggingArgInternal() override = default;
  TraceLoggingArgInternal& operator=(TraceLoggingArgInternal&& other) = default;

  const char* value() const override { return ""; }

  OSP_DISALLOW_COPY_AND_ASSIGN(TraceLoggingArgInternal);
};

// Factory to create TraceLoggingArguments, as well as abstract away the
// template parameter since it can be inferred from the function call.
class TraceLoggingArgumentFactory {
 public:
  template <typename T>
  static TraceLoggingArgInternal<T> Create(const char* name, T value) {
    return TraceLoggingArgInternal<T>(name, value);
  }
};

}  // namespace internal
}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_INTERNAL_TRACE_LOGGING_USER_ARGS_H_
