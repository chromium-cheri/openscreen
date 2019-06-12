// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/network_runner.h"

#include "platform/base/task_runner_impl.h"

namespace openscreen {
namespace platform {

// static
NetworkRunner* NetworkRunner::GetSingleton() {
  return NetworkRunnerImpl::singleton_.get();
}

NetworkRunnerImpl::NetworkRunnerImpl(
    std::unique_ptr<TaskRunner> task_runner,
    std::unique_ptr<NetworkLoop> network_loop) {
  task_runner_ = std::move(task_runner);
  network_loop_ = std::move(network_loop);
  network_loop_->SetTaskRunner(task_runner_.get());
}

Error NetworkRunnerImpl::ReadRepeatedly(UdpSocket* socket,
                                        UdpReadCallback* callback) {
  std::function<void(std::unique_ptr<UdpReadCallback::Packet>)> func =
      std::bind(&UdpReadCallback::OnRead, callback, std::placeholders::_1,
                this);
  return network_loop_->ReadRepeatedly(socket, func);
}

Error NetworkRunnerImpl::CancelRead(UdpSocket* socket) {
  return network_loop_->CancelRead(socket);
}

void NetworkRunnerImpl::PostPackagedTask(Task task) {
  task_runner_->PostTask(std::move(task));
}

void NetworkRunnerImpl::PostPackagedTaskWithDelay(Task task,
                                                  Clock::duration delay) {
  task_runner_->PostTaskWithDelay(std::move(task), delay);
}

void NetworkRunnerImpl::RunUntilStopped() {
  task_thread_ = std::make_unique<std::thread>(
      std::bind(&TaskRunner::RunUntilStopped, task_runner_.get()));
  network_thread_ = std::make_unique<std::thread>(
      std::bind(&NetworkLoop::RunUntilStopped, network_loop_.get()));
}

void NetworkRunnerImpl::RequestStopSoon() {
  task_runner_->RequestStopSoon();
  network_loop_->RequestStopSoon();
}

// Suppress warning since we need a single static instance for a singleton.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#endif

std::unique_ptr<NetworkRunnerImpl> NetworkRunnerImpl::singleton_ =
  std::make_unique<NetworkRunnerImpl>(TaskRunnerImpl::Create(Clock::now),
                                      NetworkLoop::Create());

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

}  // namespace platform
}  // namespace openscreen
