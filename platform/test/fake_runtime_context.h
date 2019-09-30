// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_TEST_FAKE_RUNTIME_CONTEXT_H_
#define PLATFORM_TEST_FAKE_RUNTIME_CONTEXT_H_

#include "gmock/gmock.h"
#include "platform/api/runtime_context.h"

namespace openscreen {
namespace platform {

class FakeRuntimeContext : public RuntimeContext {
 public:
  FakeRuntimeContext(TaskRunner* task_runner) : task_runner_(task_runner) {}

  TaskRunner* task_runner() override { return task_runner_; }

  void OnCreate(UdpSocket* socket) override {}
  void OnCreate(TlsConnection* socket) override {}
  void OnCreate(TlsConnectionFactory* socket) override {}
  void OnDestroy(UdpSocket* socket) override {}
  void OnDestroy(TlsConnection* socket) override {}
  void OnDestroy(TlsConnectionFactory* socket) override {}

 private:
  TaskRunner* task_runner_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_TEST_FAKE_RUNTIME_CONTEXT_H_
