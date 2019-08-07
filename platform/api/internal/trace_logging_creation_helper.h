// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_INTERNAL_TRACE_LOGGING_CREATION_HELPER_H_
#define PLATFORM_API_INTERNAL_TRACE_LOGGING_CREATION_HELPER_H_

#include <functional>
#include <iostream>
#include <memory>
#include <sstream>

#include "platform/api/logging.h"
#include "platform/api/trace_logging_types.h"
#include "platform/base/macros.h"

namespace openscreen {
namespace platform {
namespace internal {

// This class provides stack storage for an object of class T, as well as
// providing an accessor for this stack storage and ensuring that the destructor
// is correctly called upon deletion. In practice, this helper extends class T
// to add a move operator, with the difference that the T instance isn't
// destructed and reconstructed during the move. Instead, data for the instance
// is copied directly.
//
// One use of this class is to control how the compiler handles the lifetimes of
// anonymous objects when they are created with the ternary operator. When
// calling:
//
//    const auto& obj_on_the_stack = condition? Foo(args) : Foo::Empty()
//
// Then Foo(args) will be copied to obj_on_the_stack and the original will be
// destructed. This can be avoided through the use of static_cast<...> calls, as
// this destruction should not occur if Foo(args) is of the same type as
// obj_on_the_stack, but calling static_cast<...> in this way results in
// undefined behavior, and this call will only work with specific compilers.
//
// T = the class type being stored
// size = size of memory block to allocate for the object
template <class T, uint32_t size = sizeof(T)>
class ScopedStackStorage {
 public:
  ScopedStackStorage() : is_initialized_(false){};
  ScopedStackStorage(ScopedStackStorage&& other)
      : is_initialized_(other.is_initialized_) {
    if (other.is_initialized_) {
      memcpy(storage_, other.storage_, size);
      other.is_initialized_ = false;
    }
  }
  ~ScopedStackStorage() {
    if (is_initialized_) {
      reinterpret_cast<T*>(&storage_)->~T();
    }
  }

  T* operator->() const { return reinterpret_cast<T*>(&storage_); }

  bool is_empty() const { return !is_initialized_; }

  // Creates a new ScopedStackStorage and returns it to the caller.
  // Child = concrete type descending from T that should be created in the
  // ScopedStackStorage object.
  template <typename Child = T, typename... Args>
  static ScopedStackStorage<T, size> Create(Args... args) {
    ScopedStackStorage<T, size> storage;
    ScopedStackStorage<T, size>::Assign<Child>(&storage,
                                               std::forward<Args&&>(args)...);
    return storage;
  }

  // Assigns a ScopedStackStorage with a new Child created using the given
  // parameters.
  // Child = concrete type descending from T that should be created in the
  // ScopedStackStorage object.
  template <typename Child = T, typename... Args>
  static void Assign(ScopedStackStorage<T, size>* storage, Args... args) {
    static_assert(size >= sizeof(Child),
                  "size must be large enough to hold the provided type");
    OSP_DCHECK(!storage->is_initialized_);

    new (storage->storage_) Child(std::forward<Args&&>(args)...);
    storage->is_initialized_ = true;
  }

 private:
  alignas(alignof(T)) uint8_t storage_[size];
  bool is_initialized_;

  OSP_DISALLOW_COPY_AND_ASSIGN(ScopedStackStorage);
};

// This class hides some of the complexity of the above ScopedStackStorage
// class to allow for simpler creation of tracing objects, and additionally
// handles assigning the user-provided arguments inside of the trace logging
// classes when provided. The template parameter is the class being stored.
template <class T>
class TraceCreationHelper {
 public:
  template <typename... Args>
  static ScopedStackStorage<T> Create(Args... args) {
    return ScopedStackStorage<T>::Create(std::forward<Args>(args)...);
  }

  static ScopedStackStorage<T> Empty() { return ScopedStackStorage<T>(); }
};

}  // namespace internal
}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_INTERNAL_TRACE_LOGGING_CREATION_HELPER_H_
