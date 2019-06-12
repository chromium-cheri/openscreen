// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/network_loop.h"

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

// static
std::unique_ptr<NetworkLoop> NetworkLoop::Create() {
  return std::unique_ptr<NetworkLoop>(new NetworkLoop(
      EventWaiter::Create(), SocketHandler::Create(), SocketHandler::Create()));
}

NetworkLoop::NetworkLoop(std::unique_ptr<EventWaiter> waiter,
                         std::unique_ptr<SocketHandler> read_handler,
                         std::unique_ptr<SocketHandler> write_handler) {
  waiter_ = std::move(waiter);
  read_handler_ = std::move(read_handler);
  write_handler_ = std::move(write_handler);
  task_runner_ = nullptr;
  continue_network_processing_ = true;
}

NetworkLoop::~NetworkLoop() = default;

void NetworkLoop::SetTaskRunner(TaskRunner* task_runner) {
  task_runner_ = task_runner;
}

Error NetworkLoop::ReadRepeatedly(
    UdpSocket* socket,
    std::function<void(std::unique_ptr<UdpReadCallback::Packet>)> callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  return !read_callbacks_.insert({socket, std::move(callback)}).second
             ? Error::Code::kIOFailure
             : Error::None();
}

Error NetworkLoop::CancelRead(UdpSocket* socket) {
  std::lock_guard<std::mutex> lock(mutex_);
  return read_callbacks_.erase(socket) == 0 ? Error::Code::kNoItemFound
                                            : Error::None();
}

Error NetworkLoop::WriteAll(UdpSocket* socket, std::function<void()> callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  return !write_callbacks_.insert({socket, std::move(callback)}).second
             ? Error::Code::kIOFailure
             : Error::None();
}

Error NetworkLoop::CancelWriteAll(UdpSocket* socket) {
  std::lock_guard<std::mutex> lock(mutex_);
  return write_callbacks_.erase(socket) == 0 ? Error::Code::kNoItemFound
                                             : Error::None();
}

Error NetworkLoop::Wait(Clock::duration timeout) {
  if (task_runner_ == nullptr) {
    return Error::Code::kInitializationFailure;
  }

  // Start watching all sockets with callbacks set.
  read_handler_->Clear();
  write_handler_->Clear();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& read : read_callbacks_) {
      read_handler_->Watch(*read.first);
    }
    for (const auto& write : write_callbacks_) {
      write_handler_->Watch(*write.first);
    }
  }

  // Wait for the sockets to find something intersting or for the timeout.
  // NOTE: The unique_ptr objects must be changed to standard pointers because
  // ownership cannot change to the Eventwaiter object and the called method
  // modifies the object, so a const ref is not viable.
  auto error = waiter_->WaitForEvents(timeout, read_handler_.get(),
                                      write_handler_.get());
  if (error.code() != Error::Code::kNone) {
    return error;
  }

  // Process the results.
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& read : read_callbacks_) {
      if (read_handler_->IsChanged(*read.first)) {
        std::unique_ptr<UdpReadCallback::Packet> read_packet =
            ReadData(read.first);
        auto task = ReadCallbackExecutor(std::move(read_packet), read.second);
        task_runner_->PostTask(std::move(task));
      }
    }
    for (const auto& write : write_callbacks_) {
      if (write_handler_->IsChanged(*write.first)) {
        task_runner_->PostTask(write.second);
      }
    }
  }

  return Error::Code::kNone;
}

std::unique_ptr<UdpReadCallback::Packet> NetworkLoop::ReadData(
    UdpSocket* socket) {
  // TODO(rwkeane): Use circular buffer in Socket instead of new packet.
  auto data = std::make_unique<UdpReadCallback::Packet>();
  socket->ReceiveMessage(&(*data)[0], data->size(), &data->source,
                         &data->original_destination);
  data->socket = socket;

  return data;
}

void NetworkLoop::RunUntilStopped() {
  Clock::duration timeout = Clock::duration(250 * 1000);  // 250 ms.
  for (long i = 0; continue_network_processing_; i++) {
    Wait(timeout);
  }
}

void NetworkLoop::RequestStopSoon() {
  continue_network_processing_ = false;
}

}  // namespace platform
}  // namespace openscreen
