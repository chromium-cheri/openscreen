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
NetworkRunner* NetworkRunner::GetInstance() {
  return NetworkRunnerImpl::GetInstance();
}

// static
NetworkRunnerImpl* NetworkRunnerImpl::GetInstance() {
  return NetworkRunnerImpl::singleton_;
}

// static
NetworkRunnerImpl* NetworkRunnerImpl::singleton_ =
    new NetworkRunnerImpl(std::function<TaskRunner*()>([]() {
                            return TaskRunnerImpl::Create(Clock::now).get();
                          }),
                          NetworkLoop::Create());

NetworkRunnerImpl::NetworkRunnerImpl(
    std::function<TaskRunner*()> task_runner_factory,
    std::unique_ptr<NetworkLoop> network_loop)
    : network_loop_(std::move(network_loop)),
      task_runner_set_(false),
      task_runner_factory_(task_runner_factory) {}

Error NetworkRunnerImpl::ReadRepeatedly(UdpSocket* socket,
                                        UdpReadCallback* callback) {
  std::function<void(std::unique_ptr<UdpReadCallback::Packet>)> func =
      [&callback, this](std::unique_ptr<UdpReadCallback::Packet> packet) {
        callback->OnRead(std::move(packet), this);
      };
  return network_loop_->ReadRepeatedly(socket, func);
}

Error NetworkRunnerImpl::CancelRead(UdpSocket* socket) {
  return network_loop_->CancelRead(socket);
}

Error NetworkRunnerImpl::SetTaskRunnerFactory(
    std::function<TaskRunner*()> task_runner_factory) {
  std::lock_guard<std::mutex> lock(task_runner_mutex_);
  if (task_runner_set_) {
    return Error::Code::kInitializationFailure;
  }
  task_runner_factory_ = task_runner_factory;
  return Error::None();
}

void NetworkRunnerImpl::PostPackagedTask(Task task) {
  GetTaskRunner()->PostPackagedTask(std::move(task));
}

void NetworkRunnerImpl::PostPackagedTaskWithDelay(Task task,
                                                  Clock::duration delay) {
  GetTaskRunner()->PostPackagedTaskWithDelay(std::move(task), delay);
}

void NetworkRunnerImpl::RunUntilStopped(bool is_async) {
  const bool was_running = is_running_.exchange(true);
  OSP_CHECK(!was_running);

  network_loop_->SetTaskRunner(GetTaskRunner());
  network_thread_ = std::make_unique<std::thread>(
      [loop = network_loop_.get()]() { loop->RunUntilStopped(); });
  GetTaskRunner()->RunUntilStopped(is_async);
}

void NetworkRunnerImpl::RequestStopSoon() {
  is_running_.exchange(false);

  network_loop_->RequestStopSoon();
  GetTaskRunner()->RequestStopSoon();
  network_thread_->join();
}

TaskRunner* NetworkRunnerImpl::GetTaskRunner() {
  if (!task_runner_set_) {
    std::lock_guard<std::mutex> lock(task_runner_mutex_);
    if (!task_runner_set_) {
      task_runner_ = std::unique_ptr<TaskRunner>(task_runner_factory_());
      task_runner_set_ = true;
    }
  }

  return task_runner_.get();
}

}  // namespace platform
}  // namespace openscreen
