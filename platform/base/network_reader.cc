// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/network_reader.h"

#include <chrono>

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {
namespace {

class ReadCallbackExecutor {
 public:
  ReadCallbackExecutor(std::unique_ptr<UdpReadCallback::Packet> data,
                       NetworkReader::Callback function)
      : function_(function) {
    data_ = std::move(data);
  }

  void operator()() { function_(std::move(data_)); }

 private:
  std::unique_ptr<UdpReadCallback::Packet> data_;
  NetworkReader::Callback function_;
};

}  // namespace

NetworkReader::NetworkReader(TaskRunner* task_runner)
    : NetworkReader(task_runner, NetworkWaiter::Create()) {}

NetworkReader::NetworkReader(TaskRunner* task_runner,
                             std::unique_ptr<NetworkWaiter> waiter)
    : waiter_(std::move(waiter)),
      task_runner_(task_runner),
      is_running_(false) {
  OSP_CHECK(task_runner_);
}

NetworkReader::~NetworkReader() = default;

Error NetworkReader::ReadRepeatedly(UdpSocket* socket, Callback callback) {
  socket->SetDeletionCallback(
      [this](UdpSocket* socket) { this->CancelRead(socket); });
  std::lock_guard<std::mutex> lock(mutex_);
  return !read_callbacks_.emplace(socket, std::move(callback)).second
             ? Error::Code::kIOFailure
             : Error::None();
}

bool NetworkReader::CancelRead(UdpSocket* socket) {
  std::lock_guard<std::mutex> lock(mutex_);
  return read_callbacks_.erase(socket) != 0;
}

Error NetworkReader::WaitAndRead(Clock::duration timeout) {
  // Get the set of all sockets we care about.
  std::vector<UdpSocket*> sockets;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& read : read_callbacks_) {
      sockets.push_back(read.first);
    }
  }

  // Wait for the sockets to find something interesting or for the timeout.
  auto changed_or_error = waiter_->AwaitSocketsReadable(sockets, timeout);
  if (changed_or_error.is_error()) {
    return changed_or_error.error();
  }

  // Process the results.
  Error error = Error::None();
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (UdpSocket* read : changed_or_error.value()) {
      auto mapped_socket = read_callbacks_.find(read);
      if (mapped_socket == read_callbacks_.end()) {
        continue;
      }

      ErrorOr<std::unique_ptr<UdpReadCallback::Packet>> read_packet =
          ReadFromSocket(mapped_socket->first);
      if (read_packet.is_error()) {
        error = read_packet.error();
        continue;
      }

      // FIXME: Investigate removing ReadCallbackExecutor.
      auto task =
          ReadCallbackExecutor(read_packet.MoveValue(), mapped_socket->second);
      task_runner_->PostTask(std::move(task));
    }
  }

  return error;
}

ErrorOr<std::unique_ptr<UdpReadCallback::Packet>> NetworkReader::ReadFromSocket(
    UdpSocket* socket) {
  // TODO(rwkeane): Use circular buffer in Socket instead of new packet.
  auto data = std::make_unique<UdpReadCallback::Packet>();
  ErrorOr<size_t> read_bytes = socket->ReceiveMessage(
      &(*data)[0], data->size(), &data->source, &data->original_destination);
  if (read_bytes.is_error()) {
    return read_bytes.error();
  }

  data->socket = socket;
  data->length = read_bytes.value();

  return data;
}

void NetworkReader::RunUntilStopped() {
  const bool was_running = is_running_.exchange(true);
  OSP_CHECK(!was_running);

  Clock::duration timeout = std::chrono::milliseconds(50);
  while (is_running_) {
    WaitAndRead(timeout);
  }
}

void NetworkReader::RequestStopSoon() {
  is_running_.store(false);
}

}  // namespace platform
}  // namespace openscreen
