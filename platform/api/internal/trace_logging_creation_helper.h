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

  static UniquePtrStackWrapper<T> Empty() { return UniquePtrStackWrapper<T>(); }
};

}  // namespace internal
}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_INTERNAL_TRACE_LOGGING_CREATION_HELPER_H_
