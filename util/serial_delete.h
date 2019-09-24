// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_SERIAL_DELETE_H_
#define UTIL_SERIAL_DELETE_H_

#include "platform/api/task_runner.h"

namespace openscreen {

template <typename Type>
class SerialDelete {
 public:
  SerialDelete(platform::TaskRunner* task_runner) : task_runner_(task_runner) {}
  void operator()(Type* pointer) const {
    // WARNING: the object leaks if the task never runs.
    task_runner_->PostTask([pointer] { delete pointer; });
  }

 private:
  platform::TaskRunner* const task_runner_;
};

template <typename Type>
class SerialDeletePtr : public std::unique_ptr<Type, SerialDelete<Type>> {
 public:
  SerialDeletePtr(platform::TaskRunner* task_runner, Type* pointer)
      : std::unique_ptr<Type, SerialDelete<Type>>(
            pointer,
            SerialDelete<Type>(task_runner)) {
    OSP_DCHECK(task_runner);
  }
  SerialDeletePtr(platform::TaskRunner* task_runner,
                  std::unique_ptr<Type> pointer)
      : std::unique_ptr<Type, SerialDelete<Type>>(
            std::move(pointer),
            SerialDelete<Type>(task_runner)) {
    OSP_DCHECK(task_runner);
  }
};

template <typename Type, typename... Args>
SerialDeletePtr<Type> MakeSerialDeletePtr(platform::TaskRunner* task_runner,
                                          Args&&... args) {
  return SerialDeletePtr<Type>(task_runner,
                               new Type(std::forward<Args>(args)...));
}

}  // namespace openscreen

#endif  // UTIL_SERIAL_DELETE_H_