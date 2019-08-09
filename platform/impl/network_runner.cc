// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/network_runner.h"

#include <iostream>

#include "platform/api/logging.h"
#include "platform/impl/task_runner.h"

namespace openscreen {
namespace platform {

NetworkRunnerImpl::NetworkRunnerImpl(std::unique_ptr<TaskRunner> task_runner)
    : NetworkRunner(),
      network_loop_(std::make_unique<NetworkReader>()),
      task_runner_(std::move(task_runner)) {}

NetworkRunnerImpl::~NetworkRunnerImpl() = default;

void NetworkRunnerImpl::OnSocketCreation(UdpSocket* socket) {
  network_loop_->WatchSocket(socket);
}

void NetworkRunnerImpl::OnSocketDeletion(UdpSocket* socket) {
  network_loop_->UnwatchSocket(socket);
}

void NetworkRunnerImpl::PostPackagedTask(Task task) {
  task_runner_->PostPackagedTask(std::move(task));
}

void NetworkRunnerImpl::PostPackagedTaskWithDelay(Task task,
                                                  Clock::duration delay) {
  task_runner_->PostPackagedTaskWithDelay(std::move(task), delay);
}

void NetworkRunnerImpl::RunUntilStopped() {
  network_loop_->RunUntilStopped();
}

void NetworkRunnerImpl::RequestStopSoon() {
  network_loop_->RequestStopSoon();
}

}  // namespace platform
}  // namespace openscreen
