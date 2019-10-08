// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_PLATFORM_CLIENT_POSIX_H_
#define PLATFORM_IMPL_PLATFORM_CLIENT_POSIX_H_

#include <atomic>
#include <mutex>
#include <thread>

#include "platform/api/platform_client.h"
#include "platform/api/time.h"
#include "platform/impl/socket_handle_waiter_posix.h"
#include "platform/impl/task_runner.h"
#include "platform/impl/tls_data_router_posix.h"
#include "platform/impl/udp_socket_reader_posix.h"
#include "util/operation_loop.h"

namespace openscreen {
namespace platform {

class PlatformClientPosix : public PlatformClient {
 public:
  ~PlatformClientPosix() override;

  // This method is expected to be called before the library is used.
  // The parameter here represents the minimum amount of time that should pass
  // between iterations of the loop used to handle networking operations. Higher
  // values will result in less time being spent on these operations, but also
  // potentially less performant networking operations.
  // NOTE: This method is NOT thread safe and should only be called from the
  // embedder thread.
  static void Create(Clock::duration min_networking_thread_loop_time);

  // Shuts down the PlatformClient instance currently stored as a singleton.
  // This method is expected to be called before program exit.
  // NOTE: This method is NOT thread safe and should only be called from the
  // embedder thread.
  static void ShutDown();

  // This method is thread-safe.
  TlsDataRouterPosix* tls_data_router();

  // This method is thread-safe.
  UdpSocketReaderPosix* udp_socket_reader();

  // PlatformClient overrides.
  TaskRunner* task_runner() override;

 private:
  PlatformClientPosix(Clock::duration min_networking_thread_loop_time);

  // This method is thread-safe.
  SocketHandleWaiterPosix* socket_handle_waiter();

  // Helper functions to use when creating and calling the OperationLoop used
  // for the networking thread.
  void PerformSocketHandleWaiterActions(Clock::duration timeout);
  void PerformTlsDataRouterActions(Clock::duration timeout);
  std::vector<std::function<void(Clock::duration)>> networking_operations();

  // Instance objects with threads are created at object-creation time.
  OperationLoop networking_loop_;
  TaskRunnerImpl task_runner_;
  std::thread networking_loop_thread_;
  std::thread task_runner_thread_;

  // Track whether the associated instance variable has been created yet.
  std::atomic_bool waiter_created_{false};
  std::atomic_bool tls_data_router_created_{false};

  // Mutex to ensure thread-safe lazy construction of class members.
  std::recursive_mutex initialization_mutex_;

  // Instance objects are created at runtime when they are first needed.
  std::unique_ptr<SocketHandleWaiterPosix> waiter_;
  std::unique_ptr<UdpSocketReaderPosix> udp_socket_reader_;
  std::unique_ptr<TlsDataRouterPosix> tls_data_router_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_PLATFORM_CLIENT_POSIX_H_
