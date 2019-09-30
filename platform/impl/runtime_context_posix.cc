// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/runtime_context_posix.h"

#include <memory>

namespace openscreen {
namespace platform {

// static
std::unique_ptr<RuntimeContext> RuntimeContext::Create() {
  return std::unique_ptr<RuntimeContext>(new RuntimeContextPosix(Clock::now));
}

RuntimeContextPosix::RuntimeContextPosix(ClockNowFunctionPtr clock_func)
    : tls_data_router_posix_(&socket_handle_waiter_),
      udp_socket_reader_(&socket_handle_waiter_),
      task_runner_(clock_func),
      task_runner_thread_(&TaskRunnerImpl::RunUntilStopped, &task_runner_),
      network_loop_thread_(&SocketHandleWaiterPosix::RunUntilStopped,
                           &socket_handle_waiter_) {}

RuntimeContextPosix::~RuntimeContextPosix() {
  socket_handle_waiter_.RequestStopSoon();
  task_runner_.RequestStopSoon();
  network_loop_thread_.join();
  task_runner_thread_.join();
}

}  // namespace platform
}  // namespace openscreen
