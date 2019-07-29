// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef PLATFORM_IMPL_NETWORK_RUNNER_FACTORY_H_
#define PLATFORM_IMPL_NETWORK_RUNNER_FACTORY_H_

#include <deque>
#include <thread>

#include "platform/api/network_runner_factory.h"
#include "platform/api/task_runner.h"
#include "platform/impl/network_runner.h"

namespace openscreen {
namespace platform {

class NetworkRunnerFactoryImpl final : public NetworkRunnerFactory {
 public:
  NetworkRunnerFactoryImpl();
  explicit NetworkRunnerFactoryImpl(std::unique_ptr<TaskRunner> task_runner);
  ~NetworkRunnerFactoryImpl() override;

  void CreateNetworkRunner() override;
  NetworkRunner* Get() override {
    OSP_CHECK(network_runner_.get()) << "NetworkRunner not yet created";
    return network_runner_.get();
  }

 private:
  std::unique_ptr<NetworkRunnerImpl> network_runner_;
  std::unique_ptr<std::thread> network_runner_thread_;
  std::unique_ptr<TaskRunner> task_runner_;
  std::unique_ptr<std::thread> task_runner_thread_;
  std::deque<TaskRunner::Task> cleanup_tasks_;

  OSP_DISALLOW_COPY_AND_ASSIGN(NetworkRunnerFactoryImpl);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_NETWORK_RUNNER_FACTORY_H_
