// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_TEST_FAKE_NETWORK_RUNNER_H_
#define PLATFORM_TEST_FAKE_NETWORK_RUNNER_H_

#include <map>

#include "platform/api/network_runner.h"

namespace openscreen {
namespace platform {

class FakeNetworkRunner : public NetworkRunner {
 public:
  // NetworkRunner overrides.
  FakeNetworkRunner() = default;
  ~FakeNetworkRunner() = default;

  Error ReadRepeatedly(UdpSocket* socket, UdpReadCallback* callback) override;

  bool CancelRead(UdpSocket* socket) override;

  void PostPackagedTask(Task task) override;

  void PostPackagedTaskWithDelay(Task task, Clock::duration delay) override;

  // Helper methods.
  void RunTasksUntilIdle();

  void PostNewPacket(std::unique_ptr<UdpReadCallback::Packet> packet);

 private:
  std::vector<Task> task_queue_;
  std::map<UdpSocket*, UdpReadCallback*> callbacks_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_TEST_FAKE_NETWORK_RUNNER_H_
