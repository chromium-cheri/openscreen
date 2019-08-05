// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_INTERNAL_TRACE_LOGGING_CREATION_HELPER_H_
#define PLATFORM_API_INTERNAL_TRACE_LOGGING_CREATION_HELPER_H_

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
constexpr int kMaxArgumentSize = 128;
constexpr const char* kNullptrString = "nullptr";
}  // namespace

// Struct to store name and value for a given user-provided argument.
class TraceLoggingArgument {
 public:
  TraceLoggingArgument(const char* name) : name_(name) {}
  TraceLoggingArgument(TraceLoggingArgument&& other) = default;
  virtual ~TraceLoggingArgument() = default;

  const char* name() const { return name_; }
  virtual const char* value() const = 0;

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

  const char* value() const override { return kNullptrString; }

 private:
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

// These template classes allow for storage of a unique_ptr on the stack and
// correctly handle its deletion when it goes out-of-scope.

// This class provides a deletion method for a unqiue_ptr which has space
// allocated on the stack rather than the heap.
template <class T>
class TraceBaseStackDeleter {
 public:
  void operator()(T* ptr) { ptr->~T(); }
};

// This class provides a wrapper around unique_ptr to allow for its allocation
// on the stack rather than the heap.
// Parent = the class type for the unique_ptr declaration
// size = size of memory block to allocate for the unqiue_ptr
template <class Parent, uint32_t size = sizeof(Parent)>
class UniquePtrStackWrapper {
 public:
  UniquePtrStackWrapper() = default;
  UniquePtrStackWrapper(UniquePtrStackWrapper&& other) = default;

  Parent* operator->() const { return data_.get(); }

  bool is_empty() const { return data_.get() == nullptr; }

  static constexpr int size_ = size;

  // Creates a new UniquePtrStackWrapper and returns it to the caller.
  // Child = concrete type descending from Parent that should be created in the
  // unique_ptr.
  template <typename Child = Parent, typename... Args>
  static UniquePtrStackWrapper<Parent, size> Create(Args... args) {
    UniquePtrStackWrapper<Parent, size> wrapper;
    UniquePtrStackWrapper<Parent, size>::Assign<Child>(
        &wrapper, std::forward<Args&&>(args)...);
    return wrapper;
  }

  // Assigns a UniquePtrStackWrapper with a new Child created using the given
  // parameters.
  // Child = concrete type descending from Parent that should be created in the
  // unique_ptr.
  template <typename Child = Parent, typename... Args>
  static void Assign(UniquePtrStackWrapper<Parent, size>* wrapper,
                     Args... args) {
    static_assert(size >= sizeof(Child),
                  "size must be large enough to hold the provided type");

    wrapper->data_ =
        std::move(std::unique_ptr<Parent, TraceBaseStackDeleter<Parent>>(
            new (wrapper->storage_) Child(std::forward<Args&&>(args)...)));
  }

 private:
  alignas(32) uint8_t storage_[size];
  std::unique_ptr<Parent, TraceBaseStackDeleter<Parent>> data_;

  OSP_DISALLOW_COPY_AND_ASSIGN(UniquePtrStackWrapper);
};

// User-defined argument type.
using UserArgument =
    UniquePtrStackWrapper<TraceLoggingArgument, kMaxArgumentSize>;

// This class hides some of the complexity of the above
// UniquePtrStackWrapper::CreationHelper class to allow for simpler creation of
// tracing objects, and additionally handles assigning the user-provided
// arguments inside of the trace logging classes when provided. The template
// parameter is the class being wrapped in a unique pointer. NOTE: This class
// must be a friend of TraceLogging class types so that the user provided
// arguments can be set in the last 2 flavors of Create(...).
template <class T>
class TraceCreationHelper {
 public:
  template <typename... Args>
  static UniquePtrStackWrapper<T> Create(Args... args) {
    return UniquePtrStackWrapper<T>::Create(std::forward<Args>(args)...);
  }

  template <typename Arg1, typename... Args>
  static UniquePtrStackWrapper<T> Create(TraceCategory::Value category,
                                         const char* name,
                                         const char* file,
                                         int line,
                                         const char* arg1_name,
                                         Arg1 arg1_value,
                                         Args... args) {
    UniquePtrStackWrapper<T> result = TraceCreationHelper<T>::Create(
        category, name, file, line, std::forward<Args>(args)...);
    UserArgument::Assign<TraceLoggingArgInternal<Arg1>>(
        &result->arg1_, arg1_name, std::forward<Arg1&&>(arg1_value));
    return result;
  }

  template <typename Arg1, typename Arg2, typename... Args>
  static UniquePtrStackWrapper<T> Create(TraceCategory::Value category,
                                         const char* name,
                                         const char* file,
                                         int line,
                                         const char* arg1_name,
                                         Arg1 arg1_value,
                                         const char* arg2_name,
                                         Arg2 arg2_value,
                                         Args... args) {
    UniquePtrStackWrapper<T> result =
        TraceCreationHelper<T>::Create(category, name, file, line, arg1_name,
                                       arg1_value, std::forward<Args>(args)...);
    UserArgument::Assign<TraceLoggingArgInternal<Arg2>>(
        &result->arg2_, arg2_name, std::forward<Arg2&&>(arg2_value));
    return result;
  }

  static UniquePtrStackWrapper<T> Empty() { return UniquePtrStackWrapper<T>(); }
};

}  // namespace internal
}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_INTERNAL_TRACE_LOGGING_CREATION_HELPER_H_
