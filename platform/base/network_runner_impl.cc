// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/network_runner_impl.h"

#include <iostream>

#include "platform/api/logging.h"
#include "platform/base/task_runner_impl.h"

namespace openscreen {
namespace platform {

// static
std::unique_ptr<NetworkRunner> NetworkRunnerImpl::Create(
    std::unique_ptr<TaskRunner> task_runner) {
  TaskRunner* task_runner_ptr = task_runner.get();
  return std::unique_ptr<NetworkRunner>(new NetworkRunnerImpl(
      std::move(task_runner), NetworkReader::Create(task_runner_ptr)));
}

NetworkRunnerImpl::NetworkRunnerImpl(
    std::unique_ptr<TaskRunner> task_runner,
    std::unique_ptr<NetworkReader> network_loop)
    : network_loop_(std::move(network_loop)),
      task_runner_(std::move(task_runner)){};

Error NetworkRunnerImpl::ReadRepeatedly(UdpSocket* socket,
                                        UdpReadCallback* callback) {
  std::function<void(std::unique_ptr<UdpReadCallback::Packet>)> func =
      [&callback, this](std::unique_ptr<UdpReadCallback::Packet> packet) {
        callback->OnRead(std::move(packet), this);
      };
  return network_loop_->ReadRepeatedly(socket, func);
}

bool NetworkRunnerImpl::CancelRead(UdpSocket* socket) {
  return network_loop_->CancelRead(socket);
}

void NetworkRunnerImpl::PostPackagedTask(Task task) {
  task_runner_->PostPackagedTask(std::move(task));
}

void NetworkRunnerImpl::PostPackagedTaskWithDelay(Task task,
                                                  Clock::duration delay) {
  task_runner_->PostPackagedTaskWithDelay(std::move(task), delay);
}

void NetworkRunnerImpl::RunUntilStopped() {
  const bool was_running = is_running_.exchange(true);
  OSP_CHECK(!was_running);

  network_loop_->RunUntilStopped();
}

void NetworkRunnerImpl::RequestStopSoon() {
  is_running_.exchange(false);

  network_loop_->RequestStopSoon();
}

}  // namespace platform
}  // namespace openscreen
