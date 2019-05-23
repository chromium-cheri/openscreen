// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/network_loop.h"

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {

NetworkLoop::NetworkLoop(EventWaiter* waiter,
                         std::function<WakeUpHandler*()> handler_factory)
    : waiter_(waiter) {
  task_runner_ = nullptr;
  waiter_ = waiter;
  wake_up_handler = handler_factory();
}

NetworkLoop::~NetworkLoop() {
  delete wake_up_handler;
}

void NetworkLoop::SetTaskRunner(TaskRunner* task_runner) {
  task_runner_ = task_runner;
}

void NetworkLoop::ReadAll(UdpSocket* socket, UdpReadCallback* callback) {
  wake_up_handler->Set();
  std::lock_guard<std::mutex> lock(this->read_mutex_);
  read_callbacks_[socket] = callback;
  waiter_->WatchUdpSocketReadable(socket);
}

void NetworkLoop::CancelReadAll(UdpSocket* socket) {
  wake_up_handler->Set();
  std::lock_guard<std::mutex> lock(this->read_mutex_);
  read_callbacks_.erase(socket);
  waiter_->StopWatchingUdpSocketReadable(socket);
}

void NetworkLoop::WriteAll(UdpSocket* socket, UdpWriteCallback* callback) {
  wake_up_handler->Set();
  std::lock_guard<std::mutex> lock(write_mutex_);
  write_callbacks_[socket] = callback;
  waiter_->WatchUdpSocketWritable(socket);
}

void NetworkLoop::CancelWriteAll(UdpSocket* socket) {
  wake_up_handler->Set();
  std::lock_guard<std::mutex> lock(write_mutex_);
  write_callbacks_.erase(socket);
  waiter_->StopWatchingUdpSocketWritable(socket);
}

Error NetworkLoop::Wait(Clock::duration timeout) {
  if (task_runner_ == nullptr) {
    return Error::Code::kInitializationFailure;
  }

  // Get all events that occured since the last loop, blocking up until
  // timeout to wait for more events.
  ErrorOr<Events> events = Error::Code::kNone;
  {
    std::lock(read_mutex_, write_mutex_);
    std::lock_guard<std::mutex> l1(read_mutex_, std::adopt_lock);
    std::lock_guard<std::mutex> l2(write_mutex_, std::adopt_lock);
    events = waiter_->WaitForEvents(timeout);
  }
  if (events.is_error()) {
    return events.error();
  }

  Error::Code result = Error::Code::kNone;

  // Process all read events.
  for (auto& read_event : events.value().udp_readable_events) {
    UdpReadCallback::Packet* read_packet = this->ReadData(read_event.socket);
    std::lock_guard<std::mutex> lock(read_mutex_);
    auto entry = read_callbacks_.find(read_event.socket);
    if (entry != read_callbacks_.end()) {
      auto task = std::bind(&UdpReadCallback::OnRead, entry->second,
                            std::move(read_packet), this);
      task_runner_->PostTask(task);
    } else {
      result = Error::Code::kSocketReadFailure;
    }
  }

  // Process all write events.
  for (auto& write_event : events.value().udp_writable_events) {
    std::lock_guard<std::mutex> lock(write_mutex_);
    auto entry = write_callbacks_.find(write_event.socket);
    if (entry != write_callbacks_.end()) {
      auto task = std::bind(&UdpWriteCallback::OnWrite, entry->second, this);
      task_runner_->PostTask(task);
    } else {
      result = Error::Code::kSocketSendFailure;
    }
  }

  return result;
}

UdpReadCallback::Packet* NetworkLoop::ReadData(UdpSocket* socket) {
  auto* packet = new UdpReadCallback::Packet();
  UdpReadCallback::Packet* data(packet);
  socket->ReceiveMessage(&data->bytes[0], data->bytes.size(), &data->source,
                         &data->original_destination);
  data->socket = socket;

  return data;
}

}  // namespace platform
}  // namespace openscreen
