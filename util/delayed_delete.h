// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_DELAYED_DELETE_H_
#define UTIL_DELAYED_DELETE_H_

#include "platform/api/task_runner.h"

namespace openscreen {

template <typename Type>
class DelayedDeleter {
 public:
  DelayedDeleter(platform::TaskRunner* task_runner)
      : task_runner_(task_runner) {}
  void operator()(Type* tracker) {
    task_runner_->PostTask([tracker] { delete tracker; });
  }

 private:
  platform::TaskRunner* const task_runner_;
};

template <typename Type>
class DelayedDeleteUniquePtr
    : public std::unique_ptr<Type, DelayedDeleter<Type>> {
 public:
  DelayedDeleteUniquePtr(platform::TaskRunner* task_runner, Type* pointer)
      : std::unique_ptr<Type, DelayedDeleter<Type>>(
            pointer,
            DelayedDeleter<Type>(task_runner)) {}
};

template <typename Type, typename... Args>
DelayedDeleteUniquePtr<Type> MakeDelayedDeleteUniquePtr(
    platform::TaskRunner* task_runner,
    Args&&... args) {
  return DelayedDeleteUniquePtr<Type>(task_runner,
                                      new Type(std::forward<Args>(args)...));
}

}  // namespace openscreen

#endif  // UTIL_DELAYED_DELETE_H_