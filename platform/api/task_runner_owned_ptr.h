// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TASK_RUNNER_OWNED_PTR_H_
#define PLATFORM_API_TASK_RUNNER_OWNED_PTR_H_

#include <cassert>
#include <future>
#include <memory>
#include <utility>

#include "platform/api/serial_delete_ptr.h"
#include "platform/api/task_runner.h"

namespace openscreen {

template <typename Type, typename DeleterType = std::default_delete<Type>>
class TaskRunner::OwnedPtr {
 public:
  OwnedPtr(const OwnedPtr& other) = delete;
  OwnedPtr& operator=(const OwnedPtr& other) = delete;

  // Wrappers around unique_ptr methods.
  inline Type* get() const {
    assert(task_runner_->IsRunningOnTaskRunner());
    return wrapped_ptr_.get();
  }

  inline Type* operator->() {
    assert(task_runner_->IsRunningOnTaskRunner());
    return wrapped_ptr_.get();
  }

  inline Type& operator*() {
    assert(task_runner_->IsRunningOnTaskRunner());
    return *wrapped_ptr_;
  }

  inline Type* release() {
    assert(task_runner_->IsRunningOnTaskRunner());
    wrapped_ptr_.release();
  }

  Type* reset() {
    if (task_runner_->IsRunningOnTaskRunner()) {
      wrapped_ptr_.reset();
    } else {
      task_runner_->PostTask([this]() mutable { wrapped_ptr_.reset(); });
    }
  }

  void swap(TaskRunner::OwnedPtr<Type, DeleterType> other) {
    if (task_runner_->IsRunningOnTaskRunner()) {
      wrapped_ptr_.swap(other.wrapped_ptr_);
    } else {
      task_runner_->PostTask(
          [this, ptr = &other]() mutable { wrapped_ptr_.swap(*ptr); });
    }
  }

 private:
  // Creation method.
  template <typename... TArgs>
  friend OwnedPtr<Type, DeleterType> CreateOwned(TaskRunner*, TArgs...);

  OwnedPtr(TaskRunner* task_runner, Type* type)
      : task_runner_(task_runner),
        wrapped_ptr_(type, SerialDelete<Type, DeleterType>(task_runner_)) {}

  TaskRunner* task_runner_;
  std::unique_ptr<Type, SerialDelete<Type, DeleterType>> wrapped_ptr_;
};

// Helpers to only enable OwnedPtr creation in the correct conditions.
template <typename T>
using is_maybe_owned =
    std::integral_constant<bool,
                           std::is_pointer<T>::value ||
                               (std::is_lvalue_reference<T>::value &&
                                !std::is_copy_constructible<T>::value)>;

template <typename... TArgs>
struct is_maybe_thread_owned;

template <typename TArg, typename... TArgs>
struct is_maybe_thread_owned<TArg, TArgs...>
    : std::conditional<is_maybe_owned<TArg>::value,
                       typename is_maybe_thread_owned<TArgs...>::type,
                       std::false_type> {};

template <>
struct is_maybe_thread_owned<> : std::true_type {};

// Helpers to eliminate lvalue refs
template <typename T, typename U = T>
T RemoveLvalueRef(U type) {
  return std::forward(type);
}

template <typename T>
T RemoveLvalueRef(const T& type) {
  return type;
}

// TODO: Allow for DeleterType.
// TODO: Decay from TaskRunner::OwnedPtr<T, U> into T* in inputs.
//
// This ctor can be called from on any thread. All parameters are either moved
// or copied in, and the task runner thread takes ownership of them all. No
// pointers may be used as parameters, and references will all be copied.
template <typename Type,
          typename... TArgs,
          typename std::enable_if<!is_maybe_thread_owned<TArgs...>::type,
                                  int>::type = 0>
TaskRunner::OwnedPtr<Type> MakeOwned(TaskRunner* task_runner, TArgs... args) {
  if (task_runner->IsRunningOnTaskRunner()) {
    return TaskRunner::OwnedPtr<Type>(
        task_runner, new Type(RemoveLvalueRef(std::forward(args))...));
  } else {
    TaskRunner::OwnedPtr<Type> owned_ptr(task_runner, nullptr);
    task_runner->PostTask([ptr = &owned_ptr, args...]() mutable {
      ptr->wrapped_ptr_.reset(new Type(RemoveLvalueRef(std::forward(args))...));
    });
  }
}

// This ctor can only be called on the task runner thread. It allows for use of
// pointers and references.
template <typename Type,
          typename... TArgs,
          typename std::enable_if<is_maybe_thread_owned<TArgs...>::type,
                                  int>::type = 0>
TaskRunner::OwnedPtr<Type> MakeOwned(TaskRunner* task_runner, TArgs... args) {
  assert(task_runner->IsRunningOnTaskRunner());
  return TaskRunner::OwnedPtr<Type>(task_runner,
                                    new Type(std::forward(args)...));
}

}  // namespace openscreen

#endif  // PLATFORM_API_TASK_RUNNER_OWNED_PTR_H_
