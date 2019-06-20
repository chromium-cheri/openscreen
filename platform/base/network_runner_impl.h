// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_NETWORK_RUNNER_IMPL_H_
#define PLATFORM_BASE_NETWORK_RUNNER_IMPL_H_

#include <mutex>  // NOLINT
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
  NetworkRunnerImpl(std::function<TaskRunner*()> task_runner_factory,
                    std::unique_ptr<NetworkLoop> network_loop);

  Error ReadRepeatedly(UdpSocket* socket, UdpReadCallback* callback) override;

  Error CancelRead(UdpSocket* socket) override;

  Error SetTaskRunnerFactory(
      std::function<TaskRunner*()> task_runner_factory) override;

  void PostPackagedTask(Task task) override;

  void PostPackagedTaskWithDelay(Task task, Clock::duration delay) override;

  void RunUntilStopped(bool is_async = true) override;

  void RequestStopSoon() override;

  static NetworkRunnerImpl* GetInstance();

 protected:
  // Objects handling actual processing of this instance's calls.
  std::unique_ptr<NetworkLoop> network_loop_;
  std::unique_ptr<TaskRunner> task_runner_;

 private:
  // Retrieves the task runner for this instance.
  TaskRunner* GetTaskRunner();

  // Setting the task runner to use for this class;
  bool task_runner_set_;
  std::function<TaskRunner*()> task_runner_factory_;
  std::mutex task_runner_mutex_;

  // Atomic so that we can perform atomic exchanges.
  std::atomic_bool is_running_;

  // Threads on which the above objects run.
  std::unique_ptr<std::thread> task_thread_;
  std::unique_ptr<std::thread> network_thread_;

  // Singleton instance of this class.
  static NetworkRunnerImpl* singleton_;

  OSP_DISALLOW_COPY_AND_ASSIGN(NetworkRunnerImpl);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_BASE_NETWORK_RUNNER_IMPL_H_
