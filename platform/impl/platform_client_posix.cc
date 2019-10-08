// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/platform_client_posix.h"

#include <mutex>

namespace openscreen {
namespace platform {
namespace {

// Timeout networking operations after 50 ms.
constexpr Clock::duration kNetworkingOperationTimeout = Clock::duration(50);

}  // namespace

PlatformClientPosix::PlatformClientPosix(
    Clock::duration min_networking_thread_loop_time)
    : networking_loop_(networking_operations(),
                       kNetworkingOperationTimeout,
                       min_networking_thread_loop_time),
      task_runner_(Clock::now),
      networking_loop_thread_(&OperationLoop::RunUntilStopped,
                              &networking_loop_),
      task_runner_thread_(&TaskRunnerImpl::RunUntilStopped, &task_runner_) {}

PlatformClientPosix::~PlatformClientPosix() {
  networking_loop_.RequestStopSoon();
  task_runner_.RequestStopSoon();
  networking_loop_thread_.join();
  task_runner_thread_.join();
}

// static
void PlatformClientPosix::Create(
    Clock::duration min_networking_thread_loop_time) {
  PlatformClient::SetInstance(
      new PlatformClientPosix(min_networking_thread_loop_time));
}

// static
void PlatformClientPosix::ShutDown() {
  PlatformClient::ShutDown();
}

UdpSocketReaderPosix* PlatformClientPosix::udp_socket_reader() {
  if (!udp_socket_reader_.get()) {
    std::lock_guard<std::recursive_mutex> lock(initialization_mutex_);
    if (!udp_socket_reader_.get()) {
      udp_socket_reader_ =
          std::make_unique<UdpSocketReaderPosix>(socket_handle_waiter());
    }
  }
  return udp_socket_reader_.get();
}

TlsDataRouterPosix* PlatformClientPosix::tls_data_router() {
  if (!tls_data_router_.get()) {
    std::lock_guard<std::recursive_mutex> lock(initialization_mutex_);
    if (!tls_data_router_.get()) {
      tls_data_router_ =
          std::make_unique<TlsDataRouterPosix>(socket_handle_waiter());
      tls_data_router_created_.store(true);
    }
  }
  return tls_data_router_.get();
}

SocketHandleWaiterPosix* PlatformClientPosix::socket_handle_waiter() {
  if (!waiter_.get()) {
    std::lock_guard<std::recursive_mutex> lock(initialization_mutex_);
    if (!waiter_.get()) {
      waiter_ = std::make_unique<SocketHandleWaiterPosix>();
      waiter_created_.store(true);
    }
  }
  return waiter_.get();
}

TaskRunner* PlatformClientPosix::task_runner() {
  return &task_runner_;
}

void PlatformClientPosix::PerformSocketHandleWaiterActions(
    Clock::duration timeout) {
  if (!waiter_created_.load()) {
    return;
  }

  socket_handle_waiter()->ProcessHandles(timeout);
}

void PlatformClientPosix::PerformTlsDataRouterActions(Clock::duration timeout) {
  if (!tls_data_router_created_.load()) {
    return;
  }

  tls_data_router()->PerformNetworkingOperations(timeout);
}

std::vector<std::function<void(Clock::duration)>>
PlatformClientPosix::networking_operations() {
  return {[this](Clock::duration timeout) {
            PerformSocketHandleWaiterActions(timeout);
          },
          [this](Clock::duration timeout) {
            PerformTlsDataRouterActions(timeout);
          }};
}

}  // namespace platform
}  // namespace openscreen
