// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/test/fake_network_runner.h"

#include <iostream>

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {
namespace {
class ReadCallbackExecutor {
 public:
  ReadCallbackExecutor(
      std::unique_ptr<UdpReadCallback::Packet> data,
      std::function<void(std::unique_ptr<UdpReadCallback::Packet>)> function)
      : function_(function) {
    data_ = std::move(data);
  }

  void operator()() { function_(std::move(data_)); }

 private:
  std::unique_ptr<UdpReadCallback::Packet> data_;
  std::function<void(std::unique_ptr<UdpReadCallback::Packet>)> function_;
};
}  // namespace

Error FakeNetworkRunner::ReadRepeatedly(UdpSocket* socket,
                                        UdpReadCallback* callback) {
  callbacks_[socket] = callback;
  return Error::Code::kNone;
}

bool FakeNetworkRunner::CancelRead(UdpSocket* socket) {
  callbacks_.erase(socket);
  return true;
}

void FakeNetworkRunner::PostNewPacket(
    std::unique_ptr<UdpReadCallback::Packet> packet) {
  auto it = callbacks_.find(packet->socket);
  if (it == callbacks_.end()) {
    return;
  }

  std::function<void(std::unique_ptr<UdpReadCallback::Packet>)> callback =
      std::bind(&UdpReadCallback::OnRead, it->second, std::placeholders::_1,
                this);
  PostTask(ReadCallbackExecutor(std::move(packet), std::move(callback)));
}

void FakeNetworkRunner::PostPackagedTask(Task task) {
  task_queue_.push_back(std::move(task));
}

void FakeNetworkRunner::PostPackagedTaskWithDelay(Task task,
                                                  Clock::duration delay) {
  task_queue_.push_back(std::move(task));
}

void FakeNetworkRunner::RunTasksUntilIdle() {
  for (auto& task : task_queue_) {
    std::cout.flush();
    std::move(task)();
    std::cout << "Done executing single task\n";
    std::cout.flush();
  }
  std::cout << "Done executing ALL tasks\n";
  std::cout.flush();
  task_queue_.clear();
}

}  // namespace platform
}  // namespace openscreen
