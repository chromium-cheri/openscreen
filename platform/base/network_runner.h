// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_NETWORK_RUNNER_IMPL_H_
#define PLATFORM_BASE_NETWORK_RUNNER_IMPL_H_

#include <thread>

#include "platform/api/network_runner.h"
#include "platform/api/task_runner.h"
#include "platform/api/time.h"
#include "platform/api/udp_socket.h"
#include "platform/base/network_loop.h"

namespace openscreen {
namespace platform {

class NetworkRunnerImpl : public NetworkRunner {
 public:
  NetworkRunnerImpl(std::unique_ptr<TaskRunner> task_runner,
                    std::unique_ptr<NetworkLoop> network_loop);

  Error ReadRepeatedly(UdpSocket* socket, UdpReadCallback* callback) override;

  Error CancelRead(UdpSocket* socket) override;

  void PostPackagedTask(Task task) override;

  void PostPackagedTaskWithDelay(Task task, Clock::duration delay) override;

  void RunUntilStopped() override;

  void RequestStopSoon() override;

  static std::unique_ptr<NetworkRunnerImpl> singleton_;

 private:
  // Objects handling actual processing of this isntance's calls.
  std::unique_ptr<NetworkLoop> network_loop_;
  std::unique_ptr<TaskRunner> task_runner_;

  // Threads on which the above objects run.
  std::unique_ptr<std::thread> task_thread_;
  std::unique_ptr<std::thread> network_thread_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_BASE_NETWORK_RUNNER_IMPL_H_
