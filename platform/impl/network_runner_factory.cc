// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "platform/impl/network_runner_factory.h"

#include "platform/api/time.h"
#include "platform/impl/network_runner.h"
#include "platform/impl/task_runner.h"

namespace openscreen {
namespace platform {

NetworkRunnerFactoryImpl::NetworkRunnerFactoryImpl() {
  auto task_runner = std::make_unique<TaskRunnerImpl>(Clock::now);
  task_runner_thread_ = std::make_unique<std::thread>(
      [ptr = task_runner.get()]() { ptr->RunUntilStopped(); });
  cleanup_tasks_.emplace_back(
      [ptr = task_runner.get()]() { ptr->RequestStopSoon(); });
  task_runner_ = std::move(task_runner);
  Initialize();
}

NetworkRunnerFactoryImpl::NetworkRunnerFactoryImpl(
    std::unique_ptr<TaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {
  Initialize();
}

void NetworkRunnerFactoryImpl::Initialize() {
  auto network_runner =
      std::make_unique<NetworkRunnerImpl>(std::move(task_runner_));
  network_runner_thread_ = std::make_unique<std::thread>(
      [ptr = network_runner.get()]() { ptr->RunUntilStopped(); });
  cleanup_tasks_.emplace_back(
      [ptr = network_runner.get()]() { ptr->RequestStopSoon(); });
  network_runner_ = std::move(network_runner);
}

NetworkRunnerFactoryImpl::~NetworkRunnerFactoryImpl() {
  while (cleanup_tasks_.size()) {
    cleanup_tasks_.front()();
    cleanup_tasks_.pop_front();
  }

  if (task_runner_thread_.get()) {
    task_runner_thread_->join();
  }
  network_runner_thread_->join();
}

// static
std::unique_ptr<NetworkRunnerFactory> NetworkRunnerFactory::Create() {
  return std::make_unique<NetworkRunnerFactoryImpl>();
}

}  // namespace platform
}  // namespace openscreen
